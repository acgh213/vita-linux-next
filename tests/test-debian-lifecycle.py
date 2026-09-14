#!/usr/bin/env python3
"""Host-only tests for the Debian SysV lifecycle scripts.

All kernel-facing commands are fixtures.  Nothing here mounts, detaches, or
writes a real device.
"""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
INIT = ROOT / "debian/overlay/etc/init.d"
LOOP = INIT / "vita-loopdetach"
ROOT_SYNC = INIT / "vita-root-sync"
CONFIRM = INIT / "vita-boot-confirm"
# insserv reads these in place of the stock headers, which is how the shutdown
# chain for a loop-backed root is corrected without patching Debian's scripts.
OVERRIDES = ROOT / "debian/insserv-overrides"


class LifecycleTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.t = Path(self.tmp.name)
        self.bin = self.t / "bin"
        self.bin.mkdir()
        self.card = self.t / "mnt" / "vita-card"
        self.card.mkdir(parents=True)
        self.mounts = self.t / "mounts"
        self.proc1 = self.t / "proc1-comm"
        self.proc1.write_text("init\n")
        self.log = self.t / "commands.log"
        self._command("mountpoint", """\
            if [ \"$1\" = \"-q\" ] && [ \"$2\" = \"$CARD_MNT\" ]; then exit 0; fi
            exit 1
        """)
        self._command("date", "printf '2026-09-13T00:00:00Z\\n'")
        self._command("sync", """\
            printf 'sync %s\\n' \"$*\" >> \"$COMMAND_LOG\"
            [ \"${SYNC_FAIL:-0}\" = 1 ] && exit 1
            exit 0
        """)

    def tearDown(self):
        self.tmp.cleanup()

    def _command(self, name, body):
        p = self.bin / name
        p.write_text("#!/bin/sh\n" + body)
        p.chmod(0o755)

    def _env(self, **extra):
        env = os.environ.copy()
        env.update({
            "PATH": f"{self.bin}:/usr/bin:/bin",
            "CARD_MNT": str(self.card),
            "PROC_MOUNTS": str(self.mounts),
            "PROC1_COMM": str(self.proc1),
            "COMMAND_LOG": str(self.log),
            "RECORD": str(self.card / "boot-attempt.log"),
        })
        env.update({k: str(v) for k, v in extra.items()})
        return env

    def _run(self, script, action, **env):
        return subprocess.run(
            ["/bin/sh", str(script), action],
            env=self._env(**env), text=True, capture_output=True,
        )

    def test_scripts_exist_and_have_shell_syntax(self):
        for script in (LOOP, ROOT_SYNC, CONFIRM):
            self.assertTrue(script.is_file(), script)
            result = subprocess.run(["/bin/dash", "-n", str(script)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)

    def test_loop_detach_keeps_root_and_rejects_path_prefix(self):
        state = self.t / "loops"
        state.write_text("""/dev/loop7: []: (%s/debian-trial.img)
/dev/loop8: []: (%s/toolkit.sqsh)
/dev/loop9: []: (%s-old/wrong.img)
/dev/loop10: []: (/tmp/unrelated.img)
""" % (self.card, self.card, self.card))
        self.mounts.write_text("""/dev/loop7 / ext4 rw,relatime 0 0
/dev/loop8 /opt/toolkit squashfs ro,relatime 0 0
%s /mnt/vita-card exfat rw,relatime 0 0
""" % (self.t / "sda1"))
        self._command("losetup", """\
            if [ \"$1\" = -a ]; then cat \"$LOOP_STATE\"; exit 0; fi
            if [ \"$1\" = -d ]; then
                printf 'detach %s\\n' \"$2\" >> \"$COMMAND_LOG\"
                grep -v \"^$2:\" \"$LOOP_STATE\" > \"$LOOP_STATE.new\" && mv \"$LOOP_STATE.new\" \"$LOOP_STATE\"
                exit 0
            fi
            exit 2
        """)
        self._command("umount", "printf 'umount %s\\n' \"$1\" >> \"$COMMAND_LOG\"")
        result = self._run(LOOP, "stop", LOOP_STATE=state)
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        commands = self.log.read_text()
        self.assertIn(f"detach /dev/loop8", commands)
        self.assertIn("umount /opt/toolkit", commands)
        self.assertNotIn("detach /dev/loop7", commands)
        self.assertNotIn("detach /dev/loop9", commands)
        self.assertNotIn("detach /dev/loop10", commands)
        self.assertNotIn("fuser", commands)

    def test_loop_detach_fails_loudly_without_killing_mount_users(self):
        state = self.t / "loops"
        state.write_text(f"/dev/loop8: []: ({self.card}/toolkit.sqsh)\n")
        self.mounts.write_text(f"/dev/loop8 /opt/toolkit squashfs ro 0 0\n{self.t}/sda1 /mnt/vita-card exfat rw 0 0\n")
        self._command("losetup", """\
            if [ \"$1\" = -a ]; then cat \"$LOOP_STATE\"; exit 0; fi
            if [ \"$1\" = -d ]; then exit 0; fi
            exit 2
        """)
        self._command("umount", "exit 1")
        result = self._run(LOOP, "stop", LOOP_STATE=state)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("FAILED", result.stdout)
        self.assertNotIn("fuser", self.log.read_text() if self.log.exists() else "")

    def test_root_sync_requires_ro_root_then_records_synced(self):
        self.mounts.write_text(f"/dev/loop7 / ext4 ro,relatime 0 0\n{self.t}/sda1 {self.card} exfat rw,relatime 0 0\n")
        result = self._run(ROOT_SYNC, "stop")
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        record = (self.card / "boot-attempt.log").read_text()
        self.assertTrue(record.startswith("shutdown-synced "), record)
        self.assertIn(f"sync -f {self.card}", self.log.read_text())

    def test_root_sync_refuses_rw_root_and_does_not_claim_success(self):
        self.mounts.write_text(f"/dev/loop7 / ext4 rw,relatime 0 0\n{self.t}/sda1 {self.card} exfat rw,relatime 0 0\n")
        result = self._run(ROOT_SYNC, "stop")
        self.assertNotEqual(result.returncode, 0)
        record = (self.card / "boot-attempt.log").read_text()
        self.assertIn("unclean-shutdown", record)
        self.assertNotIn("shutdown-synced", record)

    def _boot_fixtures(self, mounts=None, ss=None, ip=None):
        self.mounts.write_text(mounts or f"/dev/loop7 / ext4 rw,relatime 0 0\n/dev/sda1 {self.card} exfat rw,relatime 0 0\n")
        self._command("ss", f"printf '%s\\n' {ss or repr('LISTEN 0 128 0.0.0.0:22 0.0.0.0:*')}")
        self._command("ip", f"""\
            case \"$*\" in
              '-4 -o addr show scope global') printf '%s\\n' '2: eth0    inet 192.0.2.10/24 brd 192.0.2.255 scope global eth0' ;;
              '-4 route show default') printf '%s\\n' 'default via 192.0.2.1 dev eth0' ;;
            esac
        """)
        if ip is not None:
            self._command("ip", ip)

    def test_boot_confirm_checks_all_usability_gates_and_syncs_append(self):
        self._boot_fixtures()
        result = self._run(CONFIRM, "start")
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        record = (self.card / "boot-attempt.log").read_text()
        self.assertTrue(record.startswith("confirmed "), record)
        self.assertIn(f"sync -f {self.card}", self.log.read_text())

    def test_boot_confirm_refuses_without_ssh_or_non_loopback_default_route(self):
        self._boot_fixtures(ss=repr('LISTEN 0 128 0.0.0.0:2222 0.0.0.0:*'))
        result = self._run(CONFIRM, "start")
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse((self.card / "boot-attempt.log").exists())
        self._boot_fixtures()
        self._command("ip", "printf '%s\\n' '1: lo inet 127.0.0.1/8 scope host lo'")
        result = self._run(CONFIRM, "start")
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse((self.card / "boot-attempt.log").exists())

    def test_boot_confirm_refuses_bad_root_or_pid1_or_readonly_card(self):
        self._boot_fixtures(mounts=f"/dev/loop7 / ext4 ro,relatime 0 0\n/dev/sda1 {self.card} exfat rw,relatime 0 0\n")
        self.assertNotEqual(self._run(CONFIRM, "start").returncode, 0)
        self.assertFalse((self.card / "boot-attempt.log").exists())

        self._boot_fixtures()
        self.proc1.write_text("not-init\n")
        self.assertNotEqual(self._run(CONFIRM, "start").returncode, 0)
        self.assertFalse((self.card / "boot-attempt.log").exists())

        self.proc1.write_text("init\\n")
        self._boot_fixtures(mounts=f"/dev/loop7 / ext4 rw,relatime 0 0\n/dev/sda1 {self.card} exfat ro,relatime 0 0\n")
        self.assertNotEqual(self._run(CONFIRM, "start").returncode, 0)
        self.assertFalse((self.card / "boot-attempt.log").exists())

    def test_boot_confirm_append_failure_is_not_success(self):
        self._boot_fixtures()
        record_dir = self.t / "record-dir"
        record_dir.mkdir()
        result = self._run(CONFIRM, "start", RECORD=record_dir)
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn("confirmed", self.log.read_text() if self.log.exists() else "")

    def test_boot_confirm_sync_failure_is_not_success(self):
        self._boot_fixtures()
        result = self._run(CONFIRM, "start", SYNC_FAIL=1)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("unclean-shutdown", (self.card / "boot-attempt.log").read_text())

    def test_headers_encode_the_filesystem_stop_chain(self):
        """The chain this platform needs, expressed as insserv dependencies.

        The semantics were measured on the console, not assumed: a service A
        declaring "Required-Stop: B" makes B stop AFTER A. Verified by running
        insserv in the Debian root and reading the resulting rc0.d links:

            K02sendsigs  K03umountroot  K04vita-root-sync  K06umountfs  K07halt

        The order matters for one specific reason. The root filesystem is a loop
        file living on an exFAT volume that umountfs cannot unmount, because the
        loop device holds it open. If umountfs ran before umountroot it would
        remount that volume read-only while / was still writable, and
        umountroot's flush of / would then have nowhere to land. Stock Debian
        gets this wrong for a loop root (umountfs declares
        "Required-Stop: umountroot"), which is why three headers are overridden.
        """
        sync_text = ROOT_SYNC.read_text() if ROOT_SYNC.exists() else ""
        confirm_text = CONFIRM.read_text() if CONFIRM.exists() else ""
        overrides = {
            name: (OVERRIDES / name).read_text()
            for name in ("sendsigs", "umountroot", "umountfs")
        }

        # umountroot runs after sendsigs: processes are killed while / is mounted.
        self.assertIn("# Required-Stop:     umountroot", overrides["sendsigs"])
        # umountroot goes first of the filesystem steps, so / goes read-only while
        # its backing volume is still writable.
        self.assertIn("# Required-Stop:     umountfs vita-root-sync", overrides["umountroot"])
        self.assertIn("# Should-Stop:       halt reboot kexec", overrides["umountroot"])
        # then vita-root-sync flushes the volume, with umountfs and the terminal
        # scripts stopping after it.
        self.assertIn("# Required-Stop:     umountfs halt reboot", sync_text)
        # umountfs is last of the filesystem steps, and must precede halt.
        self.assertIn("# Required-Stop:     halt reboot", overrides["umountfs"])
        # the stock dependency that caused the hazard must not be present.
        self.assertNotIn("# Required-Stop:     umountroot", overrides["umountfs"])
        # confirmation stays last on the start side and never sleeps.
        self.assertIn("# X-Start-After:     networking ssh sshd rc.local", confirm_text)
        self.assertNotIn("sleep", confirm_text)


if __name__ == "__main__":
    unittest.main()
