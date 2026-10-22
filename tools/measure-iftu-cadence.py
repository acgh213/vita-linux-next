#!/usr/bin/env python3
"""Read-only display IRQ cadence measurement using the already-corrected timer."""
import json
import time

PATH = '/sys/kernel/debug/dri/0/iftu_state'


def sample():
    a = time.monotonic_ns()
    with open(PATH) as f:
        state = {k: int(v) for k, v in (line.split() for line in f)}
    b = time.monotonic_ns()
    return {'before_ns': a, 'after_ns': b, 'state': state}


rows = [sample()]
for _ in range(6):
    time.sleep(10)
    rows.append(sample())
a, b = rows[0], rows[-1]
delta = b['state']['vblank_count'] - a['state']['vblank_count']
minimum_ns = b['before_ns'] - a['after_ns']
maximum_ns = b['after_ns'] - a['before_ns']
print(json.dumps({'samples': rows, 'irq_delta': delta,
                  'elapsed_mid_s': (minimum_ns + maximum_ns) / 2e9,
                  'hz_lower_bound': delta * 1e9 / maximum_ns,
                  'hz_upper_bound': delta * 1e9 / minimum_ns,
                  'scope': 'software IRQ cadence against corrected kernel clock; no clock writes'}, indent=2))
