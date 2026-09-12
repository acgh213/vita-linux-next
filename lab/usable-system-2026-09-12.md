# Usable persistent system — game-card path — 2026-09-12

This is the concise integration record for the detailed hardware record in the
`sibling vita-linux-research` checkout (`lab/usable-system-2026-09-12/RESULTS.md`).
It preserves the hardware result and the boundaries needed when reading the
current rootfs work.

## Hardware-proven baseline

A PSTV completed a full power cycle and booted a usable system from the SD2Vita
game card without USB storage, a host-side setup step, or a manual mount. The
proven baseline was:

- Linux `6.12.0-g321732d0fde7`;
- rootfs change at `168082d` (`topic/usable-system-from-gamecard`);
- `S06toolkit` found the card by supported filesystem type, mounted the toolkit
  read-only at `/opt/vita-toolkit`, and mounted the persistent workspace at
  `/mnt/workspace`;
- the login environment exposed the native C toolchain and 21 tools on `PATH`.

The embedded rootfs remains a RAM-backed rescue system. The game-card workspace
is a 4 GiB filesystem image stored as a file on the exFAT card; the card itself
was not repartitioned or reformatted. A marker and a compiled program written
before the power cycle were read back and executed afterward. Mode `0600`,
`1000:1000` ownership, symlinks, hardlinks, and executable bits were also
verified on the workspace.

## Journal and replay proof

The initial image was ext2-on-disk and had no journal. It was replaced by a
host-formatted ext4 image and migrated in place on the card; the old
`workspace-ext2.old` image was retained. The live workspace then reported
`type ext4`, and `dumpe2fs` reported `has_journal`.

Journal replay was tested separately and safely on a disposable 48 MiB image:

a dirty filesystem was snapshotted without syncing, the snapshot was mounted,
and the kernel logged `EXT4-fs (loop2): recovery complete`. It mounted without
manual fsck, recovered 52 of 123 files, and had zero unreadable/corrupt files.
The loss of unsynced writes is expected: a journal guarantees filesystem
consistency, not durability for data that was never synced. Earlier lazy
unmount and loop-device-removal attempts flushed cleanly and were explicitly
not counted as crash tests. The real workspace remained untouched during the
replay test.

## Full toolchain image: negative result

The current branch's rootfs additions (`a63020d` and `78917a1`) successfully
packed an image containing `make`, `git`, `python3`, `opkg`, and `e2fsprogs`.
The measured full image was 91 MiB decompressed, 25 MiB compressed, and produced
a 31,906,472-byte `zImage`. It was uploaded over FTP and its size was verified
on the device, but after `launch PLGINLDR0` the target did not return to ping,
SSH, or FTP during roughly 30 minutes of checks. A physical power cycle was
required. This image is **not boot-proven**; the cause is unknown. The upload
and loader-size observations do not prove a size ceiling or any particular
early-kernel failure.

The 64 MiB trimmed image (`make` + `opkg` + `e2fsprogs`, without `git` or
`python3`) is built but has not been deployed or boot-proven. Neither image may
be described as the usable hardware baseline above. The next safe diagnostic is
to read the loader's HDMI progress output before retrying; the known-good
rollback remains separate.

## Evidence boundary

This record proves the old-rootfs game-card system and the journal/replay
behavior. It does not prove that the current full toolchain rootfs boots, that
the trimmed image boots, or that the current pinned kernel gitlink is identical
to the kernel used for the game-card gate. Keep those identities explicit in
status documents.
