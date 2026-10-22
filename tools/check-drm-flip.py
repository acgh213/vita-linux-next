#!/usr/bin/env python3
"""Judge saved drm-flip-probe output. No hardware access and no inferred PASS.

Usage: tools/check-drm-flip.py saved-probe.log
The caller must also verify remote exit status, binary hash and kernel identity.
"""
import json
from pathlib import Path
import re
import sys


def evaluate(text):
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
    errors = []
    labels = ["after modeset / before flip", "after flip",
              "after flip sample 2", "after flip sample 3"]
    keys = {"selector", "live_cfg", "flip_pending", "vblank_count", "scanout_byte"}
    for label in labels:
        if not keys <= blocks.get(label, {}).keys():
            errors.append(f"missing evidence keys: {label}")
    event = re.search(r"DRM_EVENT received: crtc=(\d+) sequence=(\d+) elapsed_ns=(\d+)", text)
    budget = re.search(r"event budget=(\d+) ns", text)
    if "FAIL " in text or "SETCRTC returned success" not in text:
        errors.append("probe failed or modeset did not succeed")
    if not event or not budget or int(event[3]) > int(budget[1]):
        errors.append("missing/late completion event")
    if not errors:
        before, after, *samples = [blocks[label] for label in labels]
        if before["selector"] not in (0, 1) or after["selector"] != (before["selector"] ^ 1):
            errors.append("selector did not toggle")
        for label in labels:
            b = blocks[label]
            if b["selector"] != b["live_cfg"] or b["flip_pending"]:
                errors.append(f"driver/hardware mismatch or pending flip: {label}")
        if not 1 <= after["vblank_count"] - before["vblank_count"] <= 5:
            errors.append("flip not observed within five hardware vblanks")
        if any(s["selector"] != after["selector"] for s in samples):
            errors.append("unexpected further selector switch")
        if len({s["scanout_byte"] for s in [after, *samples]}) < 2:
            errors.append("scanout movement not established")
    return {"result": "FAIL" if errors else "PASS", "errors": errors,
            "snapshots": blocks, "scope": "one modeset and one flip; not SGX, not sustained presentation"}


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("usage: check-drm-flip.py saved-probe.log")
    result = evaluate(Path(sys.argv[1]).read_text())
    print(json.dumps(result, indent=2))
    sys.exit(result["result"] != "PASS")
