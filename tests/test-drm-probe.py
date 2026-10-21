#!/usr/bin/env python3
"""Host control-flow checks only: mocked ioctl replies are NOT display evidence."""
import importlib.util
import os
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
            "-Wl,--wrap=ioctl,--wrap=mmap64,--wrap=munmap,--wrap=poll,--wrap=read",
            "-D__wrap_mmap=__wrap_mmap64", "-o", cls.exe,
        ], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_case(self, scenario, expected, message):
        r = subprocess.run(
            [self.exe, "/dev/null", "/dev/null"],
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

    def test_refuses_existing_active_crtc(self):
        self.run_case(1, 1, "refusing to replace active userspace state")

    def test_modeset_failure_stops_before_flip(self):
        self.assertNotIn("PAGE_FLIP", self.run_case(2, 1, "FAIL SETCRTC"))

    def test_poll_error_fails(self):
        self.run_case(3, 1, "no flip event within five frames")

    def test_wrong_cookie_fails(self):
        self.run_case(4, 1, "invalid flip event")


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
