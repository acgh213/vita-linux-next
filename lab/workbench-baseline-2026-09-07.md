# Vita Linux Workbench — Phase 0 baseline freeze (2026-09-07)

Scope: record the known-good artifacts and target runtime state before any
workbench implementation work. Software-only; no target mutation performed.

## Host artifacts

| Item | Value |
|---|---|
| Payload image | `/home/cassie/projects/vita-linux-r0-clean/dist/vita-toolkit-native-2026-09-02.squashfs` |
| Payload size | 10,825,728 B |
| Payload SHA-256 | `6fb29caf00b1bf614399b471e35d719dcad4bdf3a00f6a3c0eb66022aed997a2` |
| Public repo | `/home/cassie/projects/vita-linux-next` (branch `main`, clean at freeze) |
| Build worktree | `/home/cassie/projects/vita-linux-r0-clean` (branch `main`, generated artifacts untracked by design) |

## Target runtime (192.168.18.43, measured — not assumed)

```
uname -a       Linux vita 6.12.0-g1ee3edf3c90a #14 SMP Mon Sep  7 16:42:19 EDT 2026 armv7l GNU/Linux
cpu/online     0-3
MemAvailable   378584 kB
ports          22 OPEN, 1337 closed, 1338 closed   (running Linux, not VitaOS — pitfall 27)
```

Mount state at freeze:

```
/dev/sda1  on /mnt/vita-storage  type exfat    (ro,relatime,fmask=0077,dmask=0077,iocharset=utf8,errors=remount-ro)
/dev/loop0 on /opt/vita-toolkit  type squashfs (ro,nosuid,nodev,relatime,errors=continue)
```

Target payload copy `/mnt/vita-storage/vita-toolkit-native.squashfs` hashes
`6fb29caf00b1bf614399b471e35d719dcad4bdf3a00f6a3c0eb66022aed997a2` — byte-identical
to the host artifact. Mounted toolkit reports `VERSION=1.1.0` with both `MANIFEST`
and `NATIVE-TOOLCHAIN-MANIFEST` present.

## Capability probe of the running image

`/proc/filesystems` includes `squashfs`, `vfat`, `exfat` — this image supersedes the
pitfall-46 "no squashfs" state.

BusyBox applets relevant to the workbench:

| Applet | Present |
|---|---|
| `setsid` | yes |
| `flock` | yes |
| `timeout` | **no** |
| `unshare` | **no** |
| `nsenter` | **no** |

**Consequence for this milestone:** the running image is NOT the pitfall-51
`busybox-hwprobe.config` build. Any workbench command must not depend on a target-side
`timeout` or `unshare`. Bounded execution is enforced from the host side
(`timeout N ssh …`) and by in-process runtime bounds inside the native programs, not
by a target-side `timeout` wrapper.

## Rollback position

Known-good baseline image remains the B16 kernel `ed91f5ce13d1` staged on the memory
card; the currently booted `6.12.0-g1ee3edf3c90a` is the working Linux session. No
kernel change is part of this milestone — the workbench is userland-only, so rollback
is "stop the session, leave the base image untouched".

## Boundaries reaffirmed

- Public repo receives source, tests, plans, non-secret lab records.
- Build worktree receives Buildroot output, rootfs artifacts, USB staging, deployment.
- Never `git add -A` in the build worktree; never copy `board/vita/local/` into the payload.
