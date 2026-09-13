# PSTV-first Linux system architecture

Status: proposed implementation architecture, 2026-09-13. No migration is approved
by this document alone. Work is sequenced in the
[implementation plan](../plans/2026-09-13-linux-system-roadmap.md).

## User decisions recorded 2026-09-13

| Question | Decision |
|---|---|
| Primary userland | **Debian.** "musl is just… makes it weird" — glibc is the target for the first complete system. |
| Alpine / musl (**#35**) | **Welcome, not a fallback.** The user likes Alpine explicitly and is not against it. It is sequenced *after* Debian because musl ABI coverage is the real unknown, not because it is less wanted. |
| End-state storage (**#34**) | **A Linux-native partition is wanted eventually.** Image-files-on-exFAT is the prototype path, not the destination. The migration is gated (backup, rollback, VitaOS coexistence test) and is its own lane. |
| Trial image size | **32 GiB** — see the sizing rationale in the plan; the card is 111.2 GiB free, so this is cheap and native builds are the thing that eats space. |
| Handheld availability | **Vita 1000 is online and available** (VitaOS, 1337/1338 open, 22 closed on 192.168.18.36), so handheld lanes are not blocked on hardware access. |

## Recommendation

Keep our custom kernel and a small Buildroot **rescue/initramfs**. Evaluate a
**Debian armhf userland in a separate ext4 image**, first as a chroot under the
known-good Linux boot. After native package/application tests, implement an early
root-selection stage that boots into that image. This makes `/usr`, `/etc`,
`/var/lib/dpkg`, `/var/lib/apt`, and service state genuinely persistent without
reinstalling the system into RAM at every boot.

Debian is the first candidate, not a hardware-proven claim. A native ABI/init
capability failure is a decision gate, not a reason to improvise production
workarounds.

**Alpine armv7 is an approved second target ([#35](https://github.com/acgh213/vita-linux-next/issues/35)), not a consolation prize.** The user
likes Alpine and is not against using it; what makes it second is *sequencing*, not
preference — musl is not glibc ABI-compatible, so a glibc-oriented first system
answers more questions per hour. When Alpine's turn comes, the honest gates are
package coverage for the required toolset and whether anything genuinely needs
glibc (`gcompat` and a Debian chroot are the documented routes, and neither is a
free win). Never mix Debian/Alpine/Buildroot libraries or package databases into one
root — that is the mistake that produces an unexplainable system later.

The VitaOS exploit/loader remains the entry into Linux. This work does not promise
replacement firmware, direct power-on Linux, an upstream kernel, or SGX acceleration.
The loader's `ux0:` medium and the Linux SD2Vita medium must be identified separately.

## Why this, rather than extending today's patchwork

- Buildroot produces controlled root filesystem images; adding opkg does not turn
  those images into a maintained distribution repository. Hosting our own full
  dependency/ABI/security-update ecosystem is not the best use of this project's time.
- A normal distro supplies package metadata, dependency resolution, upgrades,
  development headers, GCC/C++, and userland security updates. Our responsibility
  remains the Vita kernel, boot integration, hardware services, release recipe,
  and actual device validation. Distro updates do not patch our custom kernel.
- RAM is constrained. Programs on an ext4 filesystem are demand-paged instead of
  all being extracted into the RAM root. Current free memory is not a benchmark
  of a loaded desktop or a native compiler workload.
- An immutable squashfs+overlay arrangement is viable if tested, but complicates
  package upgrades/rollback and needs a kernel option not currently enabled. It is
  not necessary for the first writable distro root.
- The previous 'scan every .ipk and install into RAM at boot' proposal is superseded.
  Package scripts, dependency order, failures, boot cost, and trust cannot be
  delegated to a directory scan. Even a curated variant needs explicit signed
  manifests and independent state handling.

## Layer contract

1. **Boot/recovery assets:** model-specific kernel/DTB/loader plus rescue initramfs.
   An explicit rescue selection must bypass all card execution and storage mutation.
   Preserve existing private rescue identity until a tested provisioning design
   replaces it. Public artifacts contain no private keys, Wi-Fi credentials or
   user authorized-key lists. Do not publish today's private full image.
2. **exFAT transport (prototype) → Linux-native partition (destination):** keep the
   current filesystem and existing files untouched for the trial, adding only a
   separately named Linux image and manifests, after backup/headroom/card-health
   approval. Prefer fully allocated images; do not assume sparse allocation behavior
   on exFAT. **The user's stated end state is a native Linux partition**, which
   removes the unjournaled-exFAT durability boundary entirely. That migration is
   gated on its own lane and its own approval — and it carries one unresolved
   question that must be answered by test, not assumption: **VitaOS reads `ux0:` as a
   FAT volume.** Whether it tolerates a second partition after the first, or needs the
   card to be one whole FAT volume, is **untested**. A card that Linux likes and
   VitaOS cannot read is a regression, not progress. PSTV has a second path worth
   knowing about: its EHCI USB host storage is already proven, so a native ext4 root
   can come from USB without touching the card at all. The handheld has no such
   option, so the handheld is where the partition question actually matters.
3. **Normal root:** distinct ext4 image, package managed. Provisional path is
   `/mnt/vita-card/linux-system/debian-armhf-trial.ext4`; a path name is not device
   identity. Select the actual SDIF1/SD card by sysfs identity plus explicit
   filesystem/marker identity. Reject multiple candidates and internal MMC media.
4. **Existing workspace:** retain `workspace.ext4`, old ext2 archive and toolkit
   bundles unchanged. Initially chroot-contained home/projects avoid implicit
   sharing. After metadata and backup gates, expose the existing workspace at
   `/srv/workspace` in the normal system without copying over or masking its data.
5. **Runtime:** `/run`, `/tmp`, `/dev/shm` remain ephemeral. Persistent `/etc`,
   account homes, machine identity, SSH and Bluetooth state belong to the normal
   ext4 root with owner/mode checks. Bound persistent logs and package caches.
6. **Recovery copies:** keep a verified offline backup and last-known-good boot
   artifact manifest. A second mutable root image is a copy, not a snapshot;
   mutable roots cannot use their original image hash as permanent runtime identity.

## Boot and failure handling

The existing background `S06toolkit` waiter remains for rescue/workbench operation.
It is NOT a late `switch_root` hook. Normal-root selection happens in PID 1's early
initramfs stage, before normal services or the background storage waiter start.

Proposed state machine:

```
rescue override? ----------------------------------------> rescue
       no
identify one approved card -> verify container/manifest -> mount ext4 root
       | missing / ambiguous / timeout / unsafe              |
       +------------------------------------------------> rescue
                                                             |
                                  validate init/ABI + move required mounts
                                                             |
                                              commit to normal-root PID 1
```

Implementation must keep the exFAT backing mount and loop device alive across
`switch_root`, with explicit mount moves and tested cleanup. Moving only the ext4
mount is not a reviewed design. Document initramfs deletion semantics and ensure no
running process retains an unintentional rescue-root reference.

- Invalid/missing manifest or image: no auto-format, no auto-repair, no fallback
  selection of some other FAT card. Enter recognizable rescue with a bounded reason.
- Pre-handoff failure: unwind trial mounts and return to rescue.
- **After executing the new PID 1, an initramfs shell cannot simply resume.** A
  one-shot trial/rescue selection and recovery from a failed PID 1 need a separately
  tested boot-control mechanism. No 'automatic rollback' claim until that passes.
- Rescue must work with SD2Vita absent and without reliance on the failed root's
  keys/config. Missing Wi-Fi credentials must not cause open-network association.
- A safe shutdown stops dependents, flushes, unmounts nested filesystems, detaches
  loops and releases the outer exFAT mount before rail/reset actions. Busy/unmount
  errors must be visible, not swallowed. The normal-root-in-loop case needs its
  own shutdown path; do not copy `S06toolkit stop` and call it solved.

## Storage integrity and SD2Vita

An ext4 journal provides filesystem recovery inside the image. It does not protect
exFAT allocation/directory metadata or guarantee that storage-controller caches
survive power loss. Preserve backups; no durability guarantee beyond the tested
conditions. Do not hot-unplug the root card. Safe removal is an explicit lifecycle,
not a rescan command.

First priorities: capture complete boot/command/detect timelines; identify media
without `mmcblkN` assumptions; check dirty media while unmounted and quiesced;
handle worker cancellation and shutdown failures. Read-only instrumentation does
not require exposing raw rail/reset controls first. Keep rail-off `-EBUSY`.
Reset/reinit while MMC owns requests is not safe merely because the handler writes
no sectors. Delay tuning follows measurement and has lower priority than correctness.

No repartitioning in this plan — but not because it is unwanted. A native ext4
partition is the **stated destination**, and it removes a whole failure layer (no
loop, no nested filesystem, no unjournaled outer boundary). It is deferred for one
hard reason: the partition layout must first be proven compatible with VitaOS reading
`ux0:`, which requires a full card backup and a restore-tested rollback. Lane H / [#34](https://github.com/acgh213/vita-linux-next/issues/34) tracks that migration so it is
not rediscovered as an argument later. **The migration is a promise to the user, not
an optional refactor.**

## Packages and useful programs

Start with distribution-supported armhf packages and validate on-device:

- Base: shell, core utilities, TLS certificates, working time/DNS, apt/dpkg.
- Interactive work: tmux, nano or vim-tiny, less, htop, file, rsync, curl, git.
- Native development: GCC, G++, binutils, libc headers, make, pkg-config; CMake
  and Ninja as the next build-system gate. TCC remains a preserved fast C tool,
  not a substitute for a C++ toolchain.
- Languages/services: Python venv, SQLite, a small localhost HTTP service, and
  a statically cross-built Go utility as an optional low-resource application.
- Display later: bounded software-rendered framebuffer/SDL demonstration followed
  by normal DRM/KMS clients when the atomic ABI is ready. No SGX dependency for
  terminal apps or basic software rendering; no desktop performance promise.

Each package tier must be installed, exercised, restarted where relevant, and
checked for persistence. Start compile jobs at `-j1`; measure RSS/OOM behavior before
raising parallelism or introducing zram/swap. Do not swap to unreviewed exFAT files.
Do not use arbitrary OpenWrt feeds just because a package says 'ARM'.

## Device and release boundaries

PSTV first. USB HID/BT/Ethernet successes are historical baselines to preserve,
not acceptance for a newly booted distro. Pairing persistence, service ownership,
DHCP/SSH startup, framebuffer handoff and OHCI teardown all need integration gates.
USB audio is its own gate; native codec support is not assumed.

Vita 1000 follows only after fresh model/firmware/loader/storage inventory. Do not
reuse PSTV rail or USB-role behavior blindly. Handheld battery telemetry is a
read-only ABI lane; suspend/charging changes require separate review. Vita 2000
remains unclaimed until it has its own hardware record. SGX, CP14 debug and PMU
sampling do not block a usable packaged Linux userland.

## Acceptance of the architecture

A normal boot reaches local login and authorized SSH; a package installed from a
verified repository, edited config and compiled program survive a graceful reboot;
no card enters rescue; invalid trial enters rescue; boot identity is unambiguous;
shutdown has no unexplained I/O errors; exact model/artifacts are recorded. A
chroot test proves userland/ABI, not boot, service management, or containment.
