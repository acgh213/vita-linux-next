#!/usr/bin/env python3
"""Synthetic parser fixtures only. Never hardware evidence."""
import importlib.util
from pathlib import Path
import unittest

p = Path(__file__).resolve().parents[1] / 'tools/check-drm-sustained.py'
spec = importlib.util.spec_from_file_location('sustained', p)
assert spec is not None and spec.loader is not None
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def fixture(elapsed=33366667):
    rows = ['SETCRTC returned success']
    def state(label, selector, count, byte):
        rows.extend([f'=== {label} (/fixture/iftu_state) ===',
                     f'selector {selector}', f'live_cfg {selector}', 'flip_pending 0',
                     f'vblank_count {count}', f'scanout_byte {byte}',
                     f'copy_count {count}', f'present_count {count}',
                     f'copy_total_ns {count*1000000}', f'wait_total_ns {count*1000000}'])
    state('after modeset / before flip', 1, 1, 0)
    for i in range(2):
        rows.append(f'DRM_EVENT received: crtc=34 sequence={i+2} elapsed_ns=16000000 '
                    f'ioctl_ns=1000000 event_wait_ns=15000000 cookie={0x4d334232+i}')
        state(f'flip sample {i+1}', i, i+2, (i+1)*5120)
    rows.append(f'total: flips=2 elapsed_ns={elapsed} cadence_hz=59.94')
    for i, label in enumerate(('after flip', 'after flip sample 2', 'after flip sample 3')):
        state(label, 1, 3, i*5120)
    return '\n'.join(rows)


class SustainedTests(unittest.TestCase):
    def test_agreeing_signals(self):
        self.assertEqual(module.evaluate(fixture())['result'], 'PASS')

    def test_low_rate_fails_even_if_printed_rate_says_fast(self):
        self.assertIn('cadence below 95% refresh', module.evaluate(fixture(100000000))['errors'])

    def test_wrong_cookie_fails(self):
        self.assertIn('cookie/CRTC mismatch', module.evaluate(
            fixture().replace(f'cookie={0x4d334232}', 'cookie=0'))['errors'])

    def test_wrong_selector_fails(self):
        bad = fixture().replace('flip sample 1 (/fixture/iftu_state) ===\nselector 0',
                                'flip sample 1 (/fixture/iftu_state) ===\nselector 1')
        self.assertEqual(module.evaluate(bad)['result'], 'FAIL')

    def test_missing_evidence_fails(self):
        self.assertEqual(module.evaluate('')['result'], 'FAIL')


if __name__ == '__main__':
    unittest.main()
