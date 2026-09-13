# Linux system architecture evidence — 2026-09-13

## Scope and source of truth

This is a planning audit, not a new deployment. Read-only SSH and source inspection,
artifact hashing and repository/API reads were performed. No console reboot,
package installation, filesystem repair, format or migration was performed here.
User-selected constraints: general-purpose Linux with normal repositories; preserve
the current exFAT card and data, prototype separate ext4 images first.

[Direct observations and command output](../../lab/system-roadmap-audit-2026-09-13/RESULTS.md)
carry the exact opkg/config and artifact findings. Earlier evidence remains dated;
this audit does not upgrade source inspection into hardware proof.

## Current baseline

- Outer reviewed base `2820f9558219abca180c923cbc591140afd98b61`;
  kernel gitlink `37b9348710dfe1751dae0ef0fd2954b2714d08d4`.
- Full 31,910,504-byte zImage successfully booted on PSTV in today's prior trial.
  Fresh SSH inspection confirms the safety kernel, RAM root, mounted exFAT card,
  read-only squashfs toolkit and ext4 workspace. Deployed identity and build
  provenance still require manifest verification before any replacement.
- Plain local opkg install/run was demonstrated earlier in RAM, not persistent root.
- Explicit `opkg -f /etc/opkg.conf -d workspace list-installed` returns success;
  implicit config discovery fails. Tracing identifies default searches under
  `/opt/vita-toolkit/etc`. The current config also warns about `lists_dir`.
  This recognizes a destination; it does not prove installation/runtime/reboot.
- Current kernel has no tested overlayfs support; enabling it remains possible.
- A rollback file's descriptive filename/manifest size is wrong. Actual bytes and
  hash, not the filename, must establish provenance before deployment.

## Source-inspected work, not deployed fixes

At this base, `buildroot-vita/board/vita/overlay/etc/init.d/S06toolkit` launches a
background waiter and stops mounts without explicit loop detach/lifetime ownership.
Storage-selection interaction with earlier USB bootstrap scripts must be resolved.
Read-only observation precedes #16 host reset changes; rail/reset/rescan are hardware
mutations even when they do not deliberately write file sectors.

A fresh checkout is not yet a demonstrated complete release build. The existing CI
checks contracts/cart/DTB gates, not all packaged userspace and native applications.
Rootfs provenance, canonical loader preflight, clean enrollment, and full build
artifacts are part of #13, not silently assumed complete by this documentation PR.

## Candidate comparison and sources

**Debian armhf is the first candidate, not an already supported Vita distribution.**
The CPU class is promising: Debian's documented baseline is ARMv7, Thumb-2 and
VFPv3-D16. A usable hardware platform additionally needs our custom kernel, actual
ELF/libc execution, package tests and the selected init's resolved kernel config.
The armhf instruction-set match alone does not establish board support.

Primary references consulted:

- [Debian 13 armhf supported hardware](https://www.debian.org/releases/trixie/armhf/ch02s01.en.html)
  — architecture requirements and distinction between CPU and platform support.
- [Buildroot manual](https://buildroot.org/downloads/manual/manual.html), section
  “Why doesn't Buildroot generate binary packages?” — Buildroot targets complete
  filesystem builds, not the dependency/reverse-dependency maintenance expected
  of a general-purpose binary distribution. opkg can still serve a controlled
  additive/rescue lane; including opkg does not create a maintained package feed.
- [Linux initramfs documentation](https://www.kernel.org/doc/html/latest/filesystems/ramfs-rootfs-initramfs.html)
  — early userspace can locate the real root and hand off to it. Our mount-lifetime,
  failure-after-exec and nested-storage shutdown requirements need implementation.
- [Linux overlayfs documentation](https://www.kernel.org/doc/html/latest/filesystems/overlayfs.html)
  — upper/work filesystem requirements; enabling overlayfs is a real alternative,
  not ruled out by its absence in this particular build.

Alpine armv7 is the fallback candidate if measured Debian results miss the target
budget. It uses musl and its own packages; do not copy either distro's libraries
into the Buildroot root. No Alpine native trial was performed in this audit.

## Explicitly not established

- Original dark-image failure caused solely by Wi-Fi.
- Nested ext4 journal makes the underlying exFAT image power-loss-safe.
- A successful chroot equals a working distro PID1 or recoverable boot.
- A backup zImage filename equals implemented automatic rollback.
- Linux/PSTV results imply Vita 1000 or Vita 2000 parity.
- Any new roadmap fixture, builder or root-selection script exists already.

These boundaries are acceptance gates in the [implementation plan](../plans/2026-09-13-linux-system-roadmap.md),
not reasons to keep adding permanent RAM-root workarounds.
