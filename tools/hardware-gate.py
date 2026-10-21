#!/usr/bin/env python3
"""Hardware gate: build artifacts -> deploy -> boot -> verify -> recorded evidence.

WHY THIS EXISTS
---------------
Every claim in this project is supposed to be tied to a dated hardware gate. Until
now that gate was a human running a build, copying files over FTP, sending
`destroy` / `launch PLGINLDR0` over a socket, and watching for SSH by hand. That
means the evidence existed only if someone happened to be paying attention, and a
regression like a non-booting loader pin could ship unnoticed.

This script is that loop, written down. It either produces a passing record on
disk or it exits non-zero saying what failed.

WHAT IT DOES
------------
1. Preconditions: the device must be sitting in VitaOS (ports 1337 and 1338 open).
   This script does not power-cycle anything - power is a physical/HA action and
   deliberately out of scope. If the device is in Linux, shut it down and power
   cycle first.
2. Deploys the loader payload to the live path, preserving the outgoing payload
   under a hash-named sibling and verifying by readback.
3. Optionally pushes a kernel zImage and DTB, also readback-verified.
4. Sends exactly one `destroy` and one `launch PLGINLDR0`.
5. Polls for SSH until the timeout. On success it captures a boot report from the
   running kernel.
6. Writes an evidence directory: `manifest.json` (machine-readable) and
   `RESULT.md` (human-readable), both containing the SHA-256 of every input.

SAFETY
------
- Refuses any remote path matching "baremetal" - that path is inert on this
  device family and deploying there makes the device look hung while running a
  stale payload.
- One launch attempt per run. It does not retry, because a failed boot usually
  means the box is in the loader and needs a power cycle, not another launch.
- Never writes to the device's preservation names destructively; the previous
  payload is parked under payload.bin.prev-<sha12>[-N].

USAGE
-----
    hardware-gate.py --payload vita-baremetal-linux-loader/vita-baremetal-linux-loader.bin \\
                     --zimage linux_vita/arch/arm/boot/zImage \\
                     --dtb    linux_vita/arch/arm/boot/dts/pstv.dtb \\
                     --host 192.168.18.43

    hardware-gate.py --payload payload.bin --dry-run     # show the plan only

Exit codes: 0 gate passed, 1 gate failed, 2 precondition not met.
"""
from __future__ import annotations

import argparse
import ftplib
import hashlib
import io
import json
import os
import socket
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

FTP_PORT = 1337
CMD_PORT = 1338
SSH_PORT = 22
LIVE_DIR = "/ux0:/linux"
PAYLOAD_NAME = "payload.bin"
INERT_MARKER = "baremetal"


# ----------------------------------------------------------------- utilities

def sha(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest()


def sha_file(p: Path) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def port_open(host: str, port: int, timeout: float = 2.0) -> bool:
    try:
        with socket.create_connection((host, port), timeout=timeout):
            return True
    except OSError:
        return False


def log(msg: str) -> None:
    print(f"[{datetime.now(timezone.utc).strftime('%H:%M:%SZ')}] {msg}", flush=True)


def git_rev(repo: Path) -> dict:
    def r(*a):
        try:
            return subprocess.run(["git", "-C", str(repo)] + list(a),
                                  capture_output=True, text=True, timeout=30).stdout.strip()
        except Exception:
            return ""
    return {"head": r("rev-parse", "HEAD"),
            "branch": r("rev-parse", "--abbrev-ref", "HEAD"),
            "describe": r("describe", "--always", "--dirty")}


# ------------------------------------------------------------------- device

def ftp_connect(host: str) -> ftplib.FTP:
    ftp = ftplib.FTP()
    ftp.connect(host, FTP_PORT, timeout=300)
    ftp.login()
    return ftp


def ftp_read(ftp: ftplib.FTP, name: str) -> bytes:
    out = io.BytesIO()
    ftp.retrbinary("RETR " + name, out.write)
    return out.getvalue()


def ftp_put_verified(ftp: ftplib.FTP, local: Path, remote: str, host: str,
                     attempts: int = 3) -> str:
    """Upload and verify by readback, retrying the whole transfer on failure.

    The Vita's FTP server drops large transfers occasionally (a 32 MB zImage is
    the usual victim, as a BrokenPipeError mid-STOR). A retry reconnects and
    restarts the transfer; verification is by readback either way, so a retry can
    never make a bad upload look good.
    """
    data = local.read_bytes()
    h = sha(data)
    last = None
    for attempt in range(1, attempts + 1):
        try:
            ftp.storbinary("STOR " + remote, io.BytesIO(data), blocksize=65536)
            back = ftp_read(ftp, remote)
            if sha(back) != h:
                raise RuntimeError(f"readback mismatch for {remote}: wrote {h}, got {sha(back)}")
            return h
        except Exception as e:                     # noqa: BLE001 - retry any transport error
            last = e
            if attempt == attempts:
                break
            log(f"    upload of {remote} failed ({type(e).__name__}), "
                f"reconnecting and retrying ({attempt}/{attempts - 1})")
            try:
                ftp.close()
            except Exception:
                pass
            ftp.connect(host, FTP_PORT, timeout=300)
            ftp.login()
            ftp.cwd(LIVE_DIR)
            time.sleep(2)
    raise RuntimeError(f"upload of {remote} failed after {attempts} attempts: {last}")


def park_and_deploy(ftp: ftplib.FTP, local: Path, host: str) -> dict:
    """Deploy the payload, parking whatever was there under a hash-named sibling."""
    try:
        current = ftp_read(ftp, PAYLOAD_NAME)
    except ftplib.error_perm:
        current = b""
    info = {"previous_sha256": sha(current) if current else None,
            "previous_bytes": len(current)}

    tmp = PAYLOAD_NAME + ".gate"
    new_sha = ftp_put_verified(ftp, local, tmp, host)

    if current:
        names = set(ftp.nlst())
        parked = f"payload.bin.prev-{sha(current)[:12]}"
        n = 2
        while parked in names:
            parked = f"payload.bin.prev-{sha(current)[:12]}-{n}"
            n += 1
        ftp.rename(PAYLOAD_NAME, parked)
        info["parked_as"] = parked
    ftp.rename(tmp, PAYLOAD_NAME)

    back = ftp_read(ftp, PAYLOAD_NAME)
    if sha(back) != new_sha:
        raise RuntimeError("deployed payload does not match candidate")
    info["deployed_sha256"] = new_sha
    info["deployed_bytes"] = len(back)
    return info


def send_cmd(host: str, cmd: str, wait: int = 3) -> str:
    with socket.create_connection((host, CMD_PORT), timeout=10) as s:
        s.sendall((cmd + "\n").encode())
        s.settimeout(wait)
        try:
            return s.recv(4096).decode(errors="replace").strip()
        except socket.timeout:
            return ""


def wait_for_takeover(host: str, timeout: int = 60, poll: float = 1.0) -> tuple[bool, float]:
    """Wait for the loader to take the box away from VitaOS.

    Immediately after `launch PLGINLDR0` the VitaOS FTP/command ports are still
    open for a short while. Treating an open FTP port as "the device fell back to
    VitaOS" at that moment is wrong - it produced a 0-second false failure the
    first time this gate ran. Takeover means both VitaOS ports have closed.
    """
    start = time.time()
    while time.time() - start < timeout:
        if not port_open(host, FTP_PORT) and not port_open(host, CMD_PORT):
            return True, time.time() - start
        time.sleep(poll)
    return False, time.time() - start


def wait_for_ssh(host: str, timeout: int, poll: int = 5) -> tuple[bool, float, str]:
    """Poll for Linux. Returns (up, seconds, reason).

    Only report "fell back to VitaOS" once the loader has genuinely taken over and
    then handed the box back - i.e. the FTP port reappears. A port that never
    closed at all is reported as "loader never started", which is a different
    failure and points at a different cause.
    """
    start = time.time()
    while time.time() - start < timeout:
        if port_open(host, SSH_PORT):
            return True, time.time() - start, "ssh"
        if port_open(host, FTP_PORT):
            return False, time.time() - start, "returned to VitaOS"
        time.sleep(poll)
    return False, time.time() - start, "loader never handed off"


def ssh_run(host: str, key: str | None, command: str, timeout: int = 40) -> str:
    args = ["ssh", "-o", "BatchMode=yes", "-o", "StrictHostKeyChecking=no",
            "-o", f"ConnectTimeout={timeout}"]
    if key:
        args += ["-i", key]
    args += [f"root@{host}", command]
    try:
        r = subprocess.run(args, capture_output=True, text=True, timeout=timeout + 20)
        return (r.stdout or "").strip() + (("\n" + r.stderr.strip()) if r.stderr.strip() else "")
    except Exception as e:
        return f"<ssh failed: {e}>"


# --------------------------------------------------------------------- main

def main() -> int:
    ap = argparse.ArgumentParser(description="Deploy and boot-verify a load on real hardware.")
    ap.add_argument("--host", default=os.environ.get("VITA_HOST"), help="device IP (or VITA_HOST)")
    ap.add_argument("--payload", required=True, help="loader payload to deploy")
    ap.add_argument("--zimage", help="optional zImage to push")
    ap.add_argument("--dtb", help="optional DTB to push")
    ap.add_argument("--ssh-key", default=os.path.expanduser("~/.ssh/id_ed25519"),
                    help="ssh key for the boot report (default ~/.ssh/id_ed25519)")
    ap.add_argument("--timeout", type=int, default=600,
                    help="seconds to wait for SSH after launch (default 600)")
    ap.add_argument("--out", default=None, help="evidence dir (default lab/gates/<utc>)")
    ap.add_argument("--loader-ref", default=None,
                    help="commit the payload was built from, if not the submodule checkout "
                         "(e.g. when built in a worktree). Recorded as provenance.")
    ap.add_argument("--dry-run", action="store_true", help="print the plan, touch nothing")
    a = ap.parse_args()

    if not a.host:
        ap.error("no host: pass --host or set VITA_HOST")
    payload = Path(a.payload)
    if not payload.exists():
        ap.error(f"payload not found: {payload}")
    if INERT_MARKER in LIVE_DIR:
        ap.error(f"refusing {LIVE_DIR}: inert path")

    stamp = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H%M%SZ")
    out = Path(a.out or f"lab/gates/{stamp}")
    repo = Path.cwd()

    inputs = {"payload": payload}
    if a.zimage:
        inputs["zImage"] = Path(a.zimage)
    if a.dtb:
        inputs["dtb"] = Path(a.dtb)

    log(f"gate {stamp}  host={a.host}")
    for name, p in inputs.items():
        if not p.exists():
            log(f"FAIL: {name} not found: {p}")
            return 2
        log(f"  {name:8} {p.name:28} {p.stat().st_size:>10} B  {sha_file(p)[:16]}")

    if a.dry_run:
        log("dry-run: would deploy payload, push extra files, then destroy + launch once")
        return 0

    # --- precondition: device in VitaOS
    if not port_open(a.host, FTP_PORT) or not port_open(a.host, CMD_PORT):
        log(f"FAIL precondition: {a.host} is not in VitaOS "
            f"(1337={port_open(a.host, FTP_PORT)}, 1338={port_open(a.host, CMD_PORT)}). "
            f"If it is running Linux, shut down and power cycle first - this gate "
            f"deliberately does not control power.")
        return 2

    record = {
        "gate": stamp,
        "host": a.host,
        "repo": git_rev(repo),
        # The submodule checkout is context, NOT proof of what produced the payload:
        # the payload may have been built from a different commit in a worktree. The
        # authoritative identifier of an input is its sha256; --loader-ref records the
        # source commit when the caller knows it.
        "submodule_checkout": git_rev(repo / "vita-baremetal-linux-loader"),
        "payload_source_ref": a.loader_ref,
        "inputs": {k: {"path": str(v), "bytes": v.stat().st_size, "sha256": sha_file(v)}
                   for k, v in inputs.items()},
        "started_utc": datetime.now(timezone.utc).isoformat(),
    }

    try:
        ftp = ftp_connect(a.host)
        ftp.cwd(LIVE_DIR)

        if a.zimage:
            h = ftp_put_verified(ftp, Path(a.zimage), "zImage", a.host)
            record["zImage_remote_sha256"] = h
            log(f"  pushed zImage          {h[:16]}")
        if a.dtb:
            h = ftp_put_verified(ftp, Path(a.dtb), Path(a.dtb).name, a.host)
            record[f"{Path(a.dtb).name}_remote_sha256"] = h
            log(f"  pushed {Path(a.dtb).name:<15} {h[:16]}")

        dep = park_and_deploy(ftp, payload, a.host)
        record["deploy"] = dep
        ftp.close()
        log(f"  deployed payload       {dep['deployed_sha256'][:16]} "
            f"(previous parked as {dep.get('parked_as')})")

        log("  destroy")
        record["destroy_response"] = send_cmd(a.host, "destroy")
        time.sleep(3)
        log("  launch PLGINLDR0")
        record["launch_response"] = send_cmd(a.host, "launch PLGINLDR0")

        launch_t = datetime.now(timezone.utc).isoformat()
        record["launch_utc"] = launch_t

        took_over, takeover_s = wait_for_takeover(a.host)
        record["loader_takeover_seconds"] = round(takeover_s, 1)
        record["loader_took_over"] = took_over
        if not took_over:
            log(f"FAIL: loader never took over - VitaOS ports still open after "
                f"{takeover_s:.0f}s (destroy/launch may not have been accepted)")
            record["result"] = "FAIL"
            record["failure_reason"] = "loader never took over"
            raise RuntimeError(f"loader never took over - VitaOS ports still open after {takeover_s:.0f}s")

        log(f"  loader took over after {takeover_s:.0f}s - waiting for Linux")
        ok, elapsed, reason = wait_for_ssh(a.host, a.timeout)
        record["seconds_to_ssh"] = round(elapsed, 1)
        record["ssh_up"] = ok
        record["wait_outcome"] = reason

        if not ok:
            log(f"FAIL: {reason} after {elapsed:.0f}s")
            record["result"] = "FAIL"
            record["failure_reason"] = reason
        else:
            log(f"  SSH up after {elapsed:.0f}s - collecting boot report")
            report = ssh_run(a.host, a.ssh_key,
                             "uname -srm; nproc; free -m | head -2; "
                             "df -h / | tail -1; ip -br a | head -5; uptime")
            record["boot_report"] = report
            record["result"] = "PASS"
            log("  boot report captured")
    except Exception as e:
        record["result"] = "FAIL"
        record["error"] = f"{type(e).__name__}: {e}"
        log(f"FAIL: {type(e).__name__}: {e}")

    record["finished_utc"] = datetime.now(timezone.utc).isoformat()
    out.mkdir(parents=True, exist_ok=True)
    (out / "manifest.json").write_text(json.dumps(record, indent=2) + "\n")

    md = [f"# Hardware gate {stamp}", "",
          f"**Result: {record['result']}**", "",
          f"- host: `{a.host}`",
          f"- seconds to SSH: {record.get('seconds_to_ssh', 'n/a')}",
          f"- repo: `{record['repo']['describe']}`",
          f"- submodule checkout: `{record['submodule_checkout']['describe']}` "
          f"(context only - not necessarily what built the payload)",
          f"- payload source ref: `{record['payload_source_ref'] or 'not declared'}`", "",
          "The authoritative identity of each input is its SHA-256 below. The checkout and",
          "source ref are provenance hints; a hash is the thing that can be re-verified.", "",
          "## Inputs", "", "| role | file | bytes | sha256 |", "|---|---|---|---|"]
    for k, v in record["inputs"].items():
        md.append(f"| {k} | `{Path(v['path']).name}` | {v['bytes']:,} | `{v['sha256']}` |")
    md += ["", "## Deploy", ""]
    dep = record.get("deploy", {})
    for k in ("previous_sha256", "previous_bytes", "parked_as", "deployed_sha256", "deployed_bytes"):
        if k in dep:
            md.append(f"- {k}: `{dep[k]}`")
    md += ["", f"- launch: {record.get('launch_utc')}"]
    if record.get("error"):
        md += ["", "## Error", "", f"```\n{record['error']}\n```"]
    if record.get("boot_report"):
        md += ["", "## Boot report", "", "```", record["boot_report"], "```"]
    if record["result"] != "PASS":
        md += ["", "## Failure notes", "",
               "- A failed gate leaves the device in the loader. Power cycle it before the next run.",
               "- Absence of SSH is not by itself a diagnosis: without a UART the loader phase is",
               "  unobservable. See docs/LOADER-OBSERVABILITY.md."]
    (out / "RESULT.md").write_text("\n".join(md) + "\n")

    log(f"recorded {out}/RESULT.md")
    log(f"GATE {record['result']}")
    return 0 if record["result"] == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
