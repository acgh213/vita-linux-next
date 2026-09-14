#!/usr/bin/env python3
"""Run real selector with only mountpoint/sync stubbed; no device access."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / 'buildroot-vita/board/vita/overlay/usr/share/vita-boot/select-root'

class SelectorTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.card = self.root / 'card'
        self.card.mkdir()
        bindir = self.root / 'bin'
        bindir.mkdir()
        for name, body in [('mountpoint', 'exit 0'), ('sync', 'exit "${SYNC_RC:-0}"')]:
            p = bindir / name
            p.write_text('#!/bin/sh\n' + body + '\n')
            p.chmod(0o755)
        self.env = dict(os.environ, CARD_MNT=str(self.card), PATH=str(bindir)+':'+os.environ['PATH'])
    def tearDown(self):
        self.card.chmod(0o700)
        self.tmp.cleanup()
    def run_selector(self):
        return subprocess.run(['dash', str(SCRIPT)], env=self.env, capture_output=True, text=True)
    def test_fresh_card_opens_normal(self):
        self.assertEqual(self.run_selector().stdout.strip(), 'normal')
        self.assertTrue((self.card/'boot-attempt.log').read_text().startswith('opened '))
    def test_unwritable_record_never_boots_normal(self):
        self.card.chmod(0o500)
        self.assertEqual(self.run_selector().stdout.strip(), 'rescue')
    def test_sync_failure_never_boots_normal(self):
        self.env['SYNC_RC'] = '1'
        self.assertEqual(self.run_selector().stdout.strip(), 'rescue')
    def test_unclean_shutdown_forces_rescue(self):
        (self.card/'boot-attempt.log').write_text('opened t\nconfirmed t\nunclean-shutdown t flush-failed\n')
        self.assertEqual(self.run_selector().stdout.strip(), 'rescue')
    def test_mixed_corrupt_record_forces_rescue(self):
        (self.card/'boot-attempt.log').write_text('opened t\ngarbage\n')
        self.assertEqual(self.run_selector().stdout.strip(), 'rescue')
    def test_nul_record_forces_rescue(self):
        (self.card/'boot-attempt.log').write_bytes(b'opened t\x00\n')
        self.assertEqual(self.run_selector().stdout.strip(), 'rescue')
    def test_normal_override_is_one_shot(self):
        override = self.card/'vita-boot-override'
        override.write_text('normal\n')
        self.assertEqual(self.run_selector().stdout.strip(), 'normal')
        self.assertFalse(override.exists())
    def test_three_open_attempts_rescue(self):
        (self.card/'boot-attempt.log').write_text('opened t\n'*3)
        self.assertEqual(self.run_selector().stdout.strip(), 'rescue')

if __name__ == '__main__':
    unittest.main()
