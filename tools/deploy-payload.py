#!/usr/bin/env python3
"""Deploy a baremetal loader payload to the path the kplugin actually reads.

WHY THIS EXISTS
---------------
On this PSTV (fw 3.60, DvaMishkiLapa-family kplugin) the live payload path is
`ux0:/linux/payload.bin`. The xerpi-family `ux0:/baremetal/payload.bin` path is
INERT here: deploying to it changes nothing, every launch runs the same
pre-existing file, and the result reads as "the device hangs" when in fact
nothing you built ever ran.

That mistake cost three deploys and produced a wrong "the 0x88E write hangs the
box" conclusion, later withdrawn. This script exists so it cannot recur: it
targets the live path by default and refuses a baremetal target outright.

Always preserves the current on-device payload first, verifies the upload by
readback, and parks the replaced payload on-device under a hash-named sibling.

USAGE
    deploy-payload.py payload.bin
    deploy-payload.py payload.bin --host 192.168.18.43
    deploy-payload.py --preserve-only
    VITA_HOST=192.168.18.43 deploy-payload.py payload.bin

FTP port 1337 is anonymous on this device; no credentials are involved.
"""
from __future__ import annotations

import argparse
import ftplib
import hashlib
import io
import os
import sys
from pathlib import Path

DEFAULT_TARGET_DIR = "/ux0:/linux"
TARGET_NAME = "payload.bin"
INERT_MARKER = "baremetal"


def sha(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest()


def connect(host: str, port: int) -> ftplib.FTP:
    ftp = ftplib.FTP()
    ftp.connect(host, port, timeout=180)
    ftp.login()
    return ftp


def read(ftp: ftplib.FTP, name: str) -> bytes:
    out = io.BytesIO()
    ftp.retrbinary("RETR " + name, out.write)
    return out.getvalue()


def preserve(ftp: ftplib.FTP, dest: Path) -> bytes:
    dest.mkdir(parents=True, exist_ok=True)
    cur = read(ftp, TARGET_NAME)
    p = dest / f"loader-payload-{sha(cur)[:12]}.bin"
    p.write_bytes(cur)
    assert sha(p.read_bytes()) == sha(cur), "preserved copy does not match"
    print(f"preserved {TARGET_NAME}: {len(cur)} bytes sha={sha(cur)} -> {p}")
    return cur


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Deploy a baremetal loader payload to the live ux0:/linux path.")
    ap.add_argument("payload", nargs="?", help="payload binary to deploy")
    ap.add_argument("--host", default=os.environ.get("VITA_HOST"),
                    help="device IP (or set VITA_HOST)")
    ap.add_argument("--port", type=int, default=1337, help="FTP port (default 1337)")
    ap.add_argument("--target-dir", default=DEFAULT_TARGET_DIR,
                    help=f"remote directory (default {DEFAULT_TARGET_DIR})")
    ap.add_argument("--preserve-dir", default="rollback-artifacts",
                    help="local directory for the preserved payload")
    ap.add_argument("--preserve-only", action="store_true",
                    help="just pull and hash the current payload")
    a = ap.parse_args()

    if not a.host:
        ap.error("no host: pass --host or set VITA_HOST")
    if INERT_MARKER in a.target_dir:
        ap.error(f"refusing {a.target_dir}: the baremetal path is INERT on this "
                 f"device series and deploying there silently does nothing")
    if not a.preserve_only and not a.payload:
        ap.error("no payload given")

    ftp = connect(a.host, a.port)
    try:
        ftp.cwd(a.target_dir)
        print(f"working dir: {ftp.pwd()}")
        current = preserve(ftp, Path(a.preserve_dir))
        if a.preserve_only:
            return 0

        cand = Path(a.payload).read_bytes()
        h = sha(cand)
        print(f"candidate: {len(cand)} bytes sha={h}")

        tmp = TARGET_NAME + ".deploy"
        ftp.storbinary("STOR " + tmp, io.BytesIO(cand), blocksize=65536)
        assert read(ftp, tmp) == cand, "readback mismatch on uploaded temp file"
        print("upload verified")

        ftp.rename(TARGET_NAME, f"payload.bin.prev-{sha(current)[:12]}")
        ftp.rename(tmp, TARGET_NAME)

        final = read(ftp, TARGET_NAME)
        assert sha(final) == h, "final readback mismatch"
        print(f"DEPLOYED {a.target_dir}/{TARGET_NAME} {len(final)} bytes sha={sha(final)}")
        print("files:", [n for n in ftp.nlst() if n.startswith("payload.bin")])
        return 0
    finally:
        ftp.close()


if __name__ == "__main__":
    sys.exit(main())
