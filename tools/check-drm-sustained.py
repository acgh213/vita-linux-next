#!/usr/bin/env python3
"""Judge saved repeated-flip evidence. Does not access hardware or infer visual output."""
import json
from pathlib import Path
import re
import statistics
import sys


def evaluate(text, refresh=59.94):
    blocks = {}
    current = None
    for line in text.splitlines():
        m = re.fullmatch(r"=== (.+) \(.+\) ===", line)
        if m:
            current = blocks.setdefault(m[1], {})
        elif current is not None:
            m = re.fullmatch(r"(\w+) (\d+)", line)
            if m:
                current[m[1]] = int(m[2])
    rows = [tuple(map(int, x)) for x in re.findall(
        r"DRM_EVENT received: crtc=(\d+) sequence=(\d+) elapsed_ns=(\d+) "
        r"ioctl_ns=(\d+) event_wait_ns=(\d+) cookie=(\d+)", text)]
    total = re.search(r"total: flips=(\d+) elapsed_ns=(\d+) cadence_hz=([\d.]+)", text)
    errors = []
    if not total or not rows:
        return {"result": "FAIL", "errors": ["missing total/events"]}
    n, elapsed = int(total[1]), int(total[2])
    if n != len(rows) or elapsed <= 0:
        errors.append("event/total mismatch")
    for i, row in enumerate(rows):
        if row[5] != 0x4d334232 + i or row[0] != rows[0][0]:
            errors.append("cookie/CRTC mismatch")
        if row[2] > 5e9 / refresh:
            errors.append("event exceeded five frames")
        if i and not 0 < (row[1] - rows[i-1][1]) % (2**32) < 2**31:
            errors.append("nonadvancing event sequence")
    before = blocks.get("after modeset / before flip", {})
    after = blocks.get("after flip", {})
    needed = {"selector", "live_cfg", "flip_pending", "vblank_count", "scanout_byte",
              "copy_count", "copy_total_ns", "present_count", "wait_total_ns"}
    for label, b in blocks.items():
        if not needed <= b.keys():
            errors.append(f"missing keys: {label}")
        elif b["selector"] != b["live_cfg"] or b["flip_pending"]:
            errors.append(f"inconsistent/pending state: {label}")
        if label.startswith("flip sample ") and "selector" in before:
            index = int(label.split()[-1])
            if b.get("selector") != (before["selector"] ^ (index % 2)):
                errors.append(f"selector parity mismatch: {label}")
    detail = {}
    if needed <= before.keys() and needed <= after.keys():
        if after["selector"] != (before["selector"] ^ (n % 2)):
            errors.append("final selector parity mismatch")
        for key in ("copy_count", "present_count"):
            if after[key] - before[key] != n:
                errors.append(f"{key} delta mismatch")
        detail = {"copy_mean_ms": (after["copy_total_ns"]-before["copy_total_ns"])/n/1e6,
                  "publish_to_irq_mean_ms": (after["wait_total_ns"]-before["wait_total_ns"])/n/1e6,
                  "raw_vblank_delta": after["vblank_count"]-before["vblank_count"]}
    else:
        errors.append("missing before/after hardware evidence")
    samples = [blocks.get(label, {}).get("scanout_byte") for label in
               ("after flip", "after flip sample 2", "after flip sample 3")]
    if None in samples or len(set(samples)) < 2:
        errors.append("scanout movement not established")
    rate = n * 1e9 / elapsed
    if rate < refresh * .95:
        errors.append("cadence below 95% refresh")
    return {"result": "FAIL" if errors else "PASS", "errors": sorted(set(errors)),
            "flips": n, "cadence_hz": rate, "reference_hz": refresh,
            "ioctl_mean_ms": statistics.mean(r[3] for r in rows)/1e6,
            "event_wait_mean_ms": statistics.mean(r[4] for r in rows)/1e6, **detail}


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("usage: check-drm-sustained.py saved-probe.log")
    result = evaluate(Path(sys.argv[1]).read_text())
    print(json.dumps(result, indent=2))
    sys.exit(result["result"] != "PASS")
