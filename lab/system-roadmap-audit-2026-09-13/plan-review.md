# Adversarial plan review — 2026-09-13

## Verdict

The architecture has the right safety intent, but it is not yet an executable or hardware-authorizable normal-root plan. The critical gaps are the post-`switch_root` failure model, shutdown ownership, the selected PID1/rootfs contract, and package/image identity. All `system/` paths in the plan are explicitly proposed; none of the named scripts or tests were present or exercised in this review.

## Blocking findings

### P0 — Normal-root handoff has no implemented recovery mechanism

Plan C names `system/initramfs/init`, `select-root.sh`, `shutdown`, and a root-selection test, but does not specify the actual Debian PID1 (systemd, SysV, or another init), its `/sbin/init` contract, or how control returns after PID1 executes. The architecture correctly says an initramfs shell cannot simply resume after PID1 failure (`linux-system.md:101-103`), yet the plan's “one-shot trial” and “recovery after bad PID1” have no boot-attempt marker, rescue-visible state, loader/user override, or tested next-boot decision. A backup filename is explicitly not rollback. Define and test a mechanism that survives a failed normal boot and defaults safely to rescue; otherwise C cannot be deployed safely.

### P0 — Shutdown is described, not connected to the real PID1

The current `S06toolkit` starts an untracked background waiter, mounts the card read-write, and suppresses every unmount error (`S06toolkit:12-21, 109-139`). It can race a stop and remount storage; it never explicitly owns loop devices. The proposed `system/initramfs/shutdown` is not shown to be invoked by Debian PID1 or by the loader. The plan must specify service/unit ownership and prove, from the normal root, dependent stop → ext4 unmount → SquashFS/loop detach → outer exFAT unmount, with busy failures visible and fail-closed. “Clean reboot/poweroff” in README.md:15-25 is a baseline claim, not evidence for this new nested-root path; status must scope it accordingly.

### P1 — Rootfs and initramfs prerequisites are incomplete

B says to use a foreign bootstrap and compare resolved kernel requirements, but does not choose the bootstrap/second-stage method, suppress all package service starts, choose PID1, or enumerate the initramfs tools and kernel options needed for exFAT, ext4, loop mounting, `blkid`, hashing, `fsck.exfat -n`, mount moves, and rescue. A chroot can pass while PID1, cgroups, devpts, udev, `/run`, networking, or shutdown fails. Add a reproducible rootfs recipe and an explicit rescue capability manifest before any native trial.

### P1 — Image/device identity is underspecified for a mutable root

“Verify container/manifest” and “sysfs + filesystem/marker identity” (`linux-system.md:58-61`) do not define the marker schema, trust anchor, clone/duplicate handling, or how an image that changes after package installation is identified. A0's content hash cannot be a permanent runtime identity for a mutable root. Specify signed/immutable selection metadata separately from mutable image state, and test ambiguous, stale, replaced, and partially written images. The captured card also reported an unclean exFAT unmount (`live-public.txt:66`); A1's health/backup gate must be a hard prerequisite with an actual read-only result, not only planned commands.

### P1 — Package acceptance does not meet the stated finish line

The finish line requires signed-repository install, **upgrade**, removal, and persistence, but B only names `apt-get update` plus a small install/list/remove exercise (`linux-system-roadmap.md:212-223`). Add an upgrade transaction, maintainer-script/restart behavior, reboot persistence, and exact repository/keyring/index provenance. The current opkg evidence is narrower: explicit `-f /etc/opkg.conf` listing succeeds, while implicit discovery fails and the config warns on `lists_dir` syntax (`RESULTS.md:35-48`). #32 must have a clean config/list-dir gate and must not be treated as package-managed distro progress.

## Handoff/status risk

The “claim #27 or #14” instruction is directionally useful, but it lacks the exact host dependencies, bootstrap command, expected artifacts, and a finite stop/decision criterion; the resume template's “next exact command” is blank. Add those before dispatch. Also distinguish “31,910,504-byte rescue zImage boots” from the unbooted 91 MiB toolchain rootfs everywhere; otherwise agents can mistake the current hardware baseline for a packaged-distro result. Keep the Debian→Alpine fallback time-boxed with explicit reject/continue criteria to prevent another open-ended architecture lane.

**Recommendation:** approve documentation only; block implementation/hardware trial on the two P0s and require B/C/D acceptance records to prove each named deliverable independently.
