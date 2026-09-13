# System roadmap audit — 2026-09-13

Status: planning and read-only inspection; no hardware reconfiguration, filesystem
repair, package installation, reboot, or partition change performed in this audit.
The full-image boot in the preceding session remains a valid hardware result.

## User decisions

Cassie selected a general-purpose Linux environment with normal package repositories,
native development, services, then graphical applications. Preserve the current
exFAT layout and data; prototype separate ext4 image files first. The earlier
storage document's `DECIDED` label did not represent approval of its architecture.

## Fresh evidence

Captured via SSH on 2026-09-13, host timestamp 14:21 EDT:

- Kernel: `6.12.0-g37b9348710df`.
- Root: RAM `rootfs`, writable; card: `/dev/mmcblk2p1` exFAT.
- Toolkit: `/dev/loop0`, squashfs, read-only; workspace: `/dev/loop1`, ext4, rw.
- RAM: `free -m` reports total 480, available 353; no swap.
- Filesystems registered: ext4, squashfs, vfat, exfat; no overlayfs entry.
- `chroot`, `switch_root`, `strace`, `opkg`, `make`, `git`, `python3` in the
  noninteractive shell's PATH. GCC/G++ not found in that PATH. The separate
  toolkit is TinyCC; a native C smoke test is not a GCC/C++ toolchain gate.
- The card's ext4 backing filesystem remains exFAT. Journalling inside the
  image cannot guarantee integrity of the outer filesystem, its allocation
  metadata, SD controller caches, or unflushed application writes.

Exact output: [live-public.txt](live-public.txt). Dmesg was filtered to storage
lifecycle lines before publication. No credentials or device serials are included.

## Corrections to earlier conclusions

### opkg destination support — prior blanket rejection withdrawn

`opkg -d workspace list-installed` fails with rc 255.
`opkg -f /etc/opkg.conf -d workspace list-installed` succeeds with rc 0.
The latter warns that `lists_dir ext /var/lib/opkg/lists` is invalid syntax.
`strace` shows default discovery opens `/etc/opkg` (absent), not the tracked
`/etc/opkg.conf`. See [opkg-config-discovery.txt](opkg-config-discovery.txt).

This is a configuration-discovery defect, not evidence that opkg rejects all
non-root destinations. The read-only test proves the destination is recognized;
it does NOT prove persistent install/upgrade/remove or reboot behavior.
The previous recommendation to reinstall every package into RAM on every boot
therefore has no demonstrated necessity. No feed-index or dependency-resolution
gate was completed by installing a single local demo package.

### Overlayfs — configuration absence is not architectural impossibility

It is absent from the running filesystem registry. It can be evaluated as a kernel
configuration change. It is not required for a writable ext4 root, so the primary
normal-distro prototype need not add it. The earlier mount probe also reused a
directory as both upper and work; it was not a valid overlay functionality test.

### Large image — success stands, causal overclaims withdrawn

The 31,910,504-byte image with hash
`0bfcbfbfe9e5150c1157bf0de7dd86aff4803f8ba465e2f360de8b7100477724`
was readback-verified and booted with SSH and storage. That directly establishes
that this image size class can boot. It does not isolate every cause of the older
unreachable image or prove its particular AP association. Cassie's observation
that the PSTV was powered on supports investigating network visibility rather
than declaring a boot failure. Nor do two SSH-ready timings establish a scaling
law between image size and decompression time: SSH includes Wi-Fi, DHCP, services,
and polling latency. Boot-stage timing needs independent capture.

### Rollback provenance — filename is wrong

Fresh file size/hash inspection of `lab/rollback-artifacts/` gives:

- `zImage.proven-24911272B`: **24,765,560 bytes**, SHA-256
  `8f9b1a82d6a8d4aadb0916801849b10f96f0a5a157766cd9055a3f21010fa543`.
- The preceding manifest mislabeled it as the 24,911,272-byte P1 image. That
  P1 image's documented hash is a different value (`08bdd9a0...`). Do not use
  this filename to select a rollback kernel or claim its embedded release.
- Full-overlay zImage: 31,910,504 bytes, SHA-256 above.
- Full cpio.zst: 27,067,149 bytes, SHA-256
  `4d3aa25b253fd05fea2929536f9c939e388415daa1282d586a8c24f1e0275f4b`.
- DTB: 6,133 bytes, SHA-256
  `72ddd25262dcbeb827059eea246fa9ea03cf56020f375665549d147106ee0317`.

Files were only measured, not renamed or deleted. All four have mode 0600.
Reconcile image identity before the next deployment, not during a failed boot.

### Runtime reset safety and crash testing

No block-device write in a sysfs handler does not imply media safety: resetting
an SDHCI host while the MMC core or mounted filesystems use it can interrupt I/O.
Rail-off refusal must remain; separate reset/rail levers need ownership and quiesce
rules. Read-only observability does not depend on exposing those levers first.

An ext4 journal replay on a disposable image in tmpfs tests that image's recovery;
it is not an end-to-end SD2Vita/exFAT power-loss guarantee. No destructive power-loss
test of the user's current card is authorized by this roadmap.

## Result boundary

This audit establishes an opkg config defect, corrects provenance and inference,
and supports a staged normal-distro feasibility trial. It does not claim Debian,
Alpine, a new PID 1, persistent packages, or an automatic fallback has run on PSTV.
The handheld was not probed; PSTV results are not handheld acceptance.
