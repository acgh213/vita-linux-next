#!/usr/bin/env python3
"""Host control-flow checks only: mocked ioctl replies are NOT display evidence."""
import importlib.util
import os
import re
import signal
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ProbeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(prefix="drm-probe-")
        cls.exe = str(Path(cls.tmp.name) / "probe")
        subprocess.run([
            "gcc", "-Wall", "-Wextra", "-Werror",
            str(ROOT / "tools/drm-flip-probe.c"),
            str(ROOT / "tests/drm-probe-mock.c"),
            "-Wl,--wrap=ioctl,--wrap=mmap64,--wrap=munmap,--wrap=poll,--wrap=read,--wrap=clock_gettime",
            "-D__wrap_mmap=__wrap_mmap64", "-o", cls.exe,
        ], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_case(self, scenario, expected, message, *args):
        r = subprocess.run(
            [self.exe, "/dev/null", "/dev/null", *args],
            env={**os.environ, "MOCK_SCENARIO": str(scenario)},
            capture_output=True, text=True, timeout=5,
        )
        self.assertEqual(r.returncode, expected, r.stdout + r.stderr)
        self.assertIn(message, r.stdout + r.stderr)
        return r.stdout

    def test_modeset_then_distinct_buffer_flip(self):
        out = self.run_case(0, 0, "DRM_EVENT received")
        self.assertLess(out.index("SETCRTC returned success"), out.index("PAGE_FLIP"))
        self.assertIn("colour=00202020", out)
        self.assertIn("colour=00ff00ff", out)
        self.assertIn("cleanup: disable probe CRTC", out)

    def test_native_abgr_is_explicit_and_opaque(self):
        out = self.run_case(0, 0, "ADDFB2 native ABGR8888", "--native-abgr", "--frames", "8")
        self.assertIn("colour=ff202020", out)
        self.assertIn("colour=ff212121", out)
        self.assertNotIn("ADDFB legacy", out)
        self.assertEqual(out.count("DRM_EVENT received"), 8)

    def test_native_format_failure_stops_before_modeset(self):
        out = self.run_case(15, 1, "FAIL ADDFB2", "--native-abgr")
        self.assertNotIn("SETCRTC (", out)

    def test_refuses_existing_active_crtc(self):
        self.run_case(1, 1, "refusing to replace active userspace state")

    def test_modeset_failure_stops_before_flip(self):
        self.assertNotIn("PAGE_FLIP", self.run_case(2, 1, "FAIL SETCRTC"))

    def test_poll_error_fails(self):
        self.run_case(3, 1, "no flip event within five frames")

    def test_wrong_cookie_fails(self):
        self.run_case(4, 1, "invalid flip event")

    def test_repeated_alternating_ids_unique_cookies_and_timings(self):
        out = self.run_case(0, 0, "threshold met", "--frames", "8")
        rows = re.findall(r"PAGE_FLIP index=(\d+) fb=(\d+) cookie=(\d+)", out)
        self.assertEqual(rows, [(str(i + 1), str(102 if i % 2 == 0 else 101),
                                str(0x4d334232 + i)) for i in range(8)])
        self.assertEqual(out.count("DRM_EVENT received"), 8)
        self.assertEqual(out.count("MOCK RMFB"), 2)
        self.assertEqual(out.count("MOCK DESTROY_DUMB"), 2)
        self.assertEqual(out.count("cleanup: disable"), 1)
        self.assertIn("total: flips=8", out)
        self.assertIn("ioctl_ns=2000000 event_wait_ns=14666667", out)
        self.assertIn("after cleanup", out)
        self.assertIn("event budget=83333334 ns", out)  # Not stale 64473 kHz.

    def test_seconds_bounded_and_frames_max(self):
        out = self.run_case(0, 0, "total: flips=60", "--seconds", "1")
        self.assertEqual(out.count("DRM_EVENT received"), 60)
        self.run_case(0, 0, "total: flips=3000", "--frames", "3000")

    def test_invalid_arguments_before_device_open(self):
        for args in [("--frames", "0"), ("--frames", "3001"), ("--seconds", "31"),
                     ("--seconds", "-1"), ("--frames", "abc"), ("--frames", "1x"),
                     ("--frames", "1", "--seconds", "1"), ("--frames",),
                     ("--refresh-hz", "0"), ("--seconds", "1.5")]:
            with self.subTest(args=args):
                out = self.run_case(0, 2, "", *args)
                self.assertNotIn("SET_MASTER", out)

    def test_slow_cadence_is_not_sustained_pass(self):
        out = self.run_case(5, 1, "BELOW threshold", "--frames", "5")
        self.assertIn("cadence_hz=20.000", out)
        self.assertIn("after cleanup", out)

    def test_repeated_event_validation_and_deadlines(self):
        for scenario, message in [(6, "invalid flip event"), (7, "nonmonotonic"),
                                  (8, "nonmonotonic"), (9, "late flip event"),
                                  (10, "five-frame budget"), (11, "invalid flip event"),
                                  (12, "invalid flip event"), (13, "invalid flip event")]:
            with self.subTest(scenario=scenario):
                self.assertIn("cleanup: disable", self.run_case(scenario, 1, message,
                                                                "--frames", "3"))

    def test_cleanup_failure_is_reported(self):
        self.run_case(14, 1, "MOCK RMFB")

    def test_explicit_refresh_controls_deadline_and_threshold(self):
        out = self.run_case(0, 0, "reference_hz=50", "--frames", "3", "--refresh-hz", "50")
        self.assertIn("event budget=100000000 ns", out)

    def test_hold_ready_can_be_killed_without_userspace_cleanup(self):
        with subprocess.Popen([self.exe, "/dev/null", "/dev/null", "--hold"],
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              text=True) as proc:
            try:
                # communicate timeout bounds waiting for the hold marker too.
                out, _ = proc.communicate(timeout=0.5)
                self.fail("hold returned early: " + out)
            except subprocess.TimeoutExpired:
                proc.kill()
                out, err = proc.communicate(timeout=2)
                self.assertEqual(proc.returncode, -signal.SIGKILL, err)
                self.assertIn("HOLD ready", out)
                self.assertNotIn("cleanup: disable", out)


class EvidenceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        spec = importlib.util.spec_from_file_location("judge", ROOT / "tools/check-drm-flip.py")
        assert spec is not None and spec.loader is not None
        cls.judge = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.judge)

    def fixture(self, toggles=True, moves=True, late=False):
        # Explicit test fixture only, not copied into hardware records.
        rows = ["SETCRTC returned success", "PAGE_FLIP event budget=100000000 ns",
                f"DRM_EVENT received: crtc=34 sequence=2 elapsed_ns={200000000 if late else 19000000}"]
        labels = ["after modeset / before flip", "after flip", "after flip sample 2", "after flip sample 3"]
        for i, label in enumerate(labels):
            sel = int(i == 0 or not toggles)
            rows.extend([f"=== {label} (/fixture/iftu_state) ===",
                         f"selector {sel}", f"live_cfg {sel}", "flip_pending 0",
                         f"vblank_count {100 + bool(i)}",
                         f"scanout_byte {i * 5120 if moves else 0}"])
        return "\n".join(rows)

    def test_agreeing_independent_signals(self):
        self.assertEqual(self.judge.evaluate(self.fixture())["result"], "PASS")

    def test_no_op_selector_fails(self):
        self.assertEqual(self.judge.evaluate(self.fixture(toggles=False))["result"], "FAIL")

    def test_frozen_scanout_fails(self):
        self.assertEqual(self.judge.evaluate(self.fixture(moves=False))["result"], "FAIL")

    def test_late_event_fails(self):
        self.assertEqual(self.judge.evaluate(self.fixture(late=True))["result"], "FAIL")

    def test_missing_driver_evidence_fails(self):
        self.assertEqual(self.judge.evaluate("SETCRTC returned success")["result"], "FAIL")


if __name__ == "__main__":
    unittest.main()
