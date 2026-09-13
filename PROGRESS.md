# Vita Linux Next — progress snapshot

**Updated:** 2026-09-13

**Canonical project:** [acgh213/vita-linux-next](https://github.com/acgh213/vita-linux-next)

**Kernel integration branch:** `acgh213/linux_vita:vita-linux-next`

**Hardware-tested kernel / current pin:** `37b9348710df`

The detailed, evidence-ranked ledger is [`docs/PROJECT-STATUS.md`](docs/PROJECT-STATUS.md). This file is the short front-page snapshot.

## Current integrated baseline

Kernel `37b9348710df` (tip of `vita-linux-next`, and the gitlink pinned by outer
`main`) carries the work already proven on PSTV hardware:

- corrected 165.6675 MHz ARM timer clock;
- production `pstv-ohci` companion driver;
- PSTV device-tree enablement;
- `vita_defconfig` enablement;
- runtime-PM handling needed for production probe.

The timer/OHCI series is in the branch history at `0d1ba53a4376`.
[`acgh213/linux_vita#10`](https://github.com/acgh213/linux_vita/pull/10) was **closed
as a no-op, not merged** — its head already equalled `vita-linux-next`, so a merge
would have produced an empty commit while implying a gate ran. **Do not reopen it.**
Kernel PRs #11 (SDIF1 game-card power) and #12 (rail-safety follow-up) are merged and
pinned by outer PRs #24 and #25. New kernel topic branches start from
`vita-linux-next`, never from the historical manual-gate branch.

## Recent hardware milestones

- `PERIPHCLK = CPU/2` measured directly with PMCCNTR versus the architectural global timer (72 ppm error); the corrected DT rate passed a 300-second RTC gate.
- ARM PMU hardware-event counting works. CP15 PMU access is available; CP14 hardware-debug access remains secure-world gated.
- Production OHCI carries real full-speed HID interrupt traffic and coexists with EHCI high-speed storage/hotplug.
- A Keychron C3 Pro provided an interactive HDMI-console login through production OHCI.
- A PixArt Lenovo USB Optical Mouse passed real 1.5 Mb/s low-speed enumeration and `hid-generic` binding.
- A HyperX Amp USB audio dongle reaches OHCI; only its HID control interface is proven so far, not audio playback.

See [`lab/usb-production-multidevice-2026-09-12.md`](lab/usb-production-multidevice-2026-09-12.md) for the concise multi-device record.

## Persistent userspace

The 2026-09-12 game-card baseline is usable and persistent on real PSTV hardware
with the old rootfs (`168082d`, kernel `6.12.0-g321732d0fde7`):

- RAM-backed rescue initramfs remains independent of removable storage;
- deterministic SquashFS toolkit mounts read-only;
- the card-mounted toolkit is hash-verified and read-only;
- a 4 GiB journalled workspace image on the card preserves POSIX metadata and survives a power cycle;
- a disposable dirty-image test mounted after `recovery complete`, with zero corrupt recovered files;
- the native C toolchain and 21 login-path tools were available without USB or host setup.

**The full toolchain rootfs is now boot-proven (2026-09-13).** The
31,910,504-byte image (`make`, `git`, `python3`, `opkg`, `e2fsprogs`, complete
8-file secret overlay, sha256 `0bfcbfbf…`) booted with SSH answering in 99 s,
`ext4` registered, `mlan0` on the correct LAN, the card enumerated at ~5.7 s, and
**341 MB RAM free of 480**. So the earlier dark boot was *not* a payload-size
limit, and boot latency simply scales with initramfs size (45 s @ 24.9 MB →
99 s @ 31.9 MB). Original failure causality is still not uniquely established —
the Wi-Fi/off-subnet explanation is consistent with the evidence, not proven by it.
Record: [`lab/retry-large-image-2026-09-13/RESULTS.md`](lab/retry-large-image-2026-09-13/RESULTS.md).
The 64 MiB trimmed image is **no longer needed** for a size reason; it stays
archived and would now have to pass
`scripts/audit-deploy-artifact-overlay.py` before any upload.

Still open — and now the main line of work: `/` is still a **RAM initramfs**, so
nothing installed survives reboot. That, not a missing tool, is the real gap. The
plan is [`docs/plans/2026-09-13-linux-system-roadmap.md`](docs/plans/2026-09-13-linux-system-roadmap.md)
under tracker [#18](https://github.com/acgh213/vita-linux-next/issues/18):
Buildroot rescue + a Debian armhf normal root in a separate ext4 image, with
storage lifecycle (#14), release foundation (#13), program tiers (#30) and
handheld parity (#31) sectioned for parallel agents. The earlier explicit USB
Workbench session/build/example gates remain valid, but must not be merged with
the game-card boot result.

## Next hardware work

1. Complete production-OHCI lifecycle acceptance: attached-child teardown, reboot/VBUS ordering, repeated boots, a true cold boot, and suspend rejection.
2. Test USB Audio Class streaming with the already-enumerating HyperX Amp.
3. Move the minimal `/pmu` node into `pstv.dts` and decide whether IRQ-backed sampling is worth wiring.
4. Bisect the exact CP14 access that dark-boots `hw_breakpoint` initialization.
5. Wire the proven IFTU/DRM flip path into normal atomic/page-flip userspace ABI.
6. Characterize Vita-handheld external USB separately from the proven PSTV Type-A path.

SGX acceleration remains on hold. The PMU/CP14 result concerns CPU debug facilities and provides no new PowerVR secure-world lever. Do not resume guessed SMC probing without new provenance.

## Public-readiness boundary

This remains an experimental hardware port, not a consumer distribution. A desktop build is not a hardware pass. Build instructions, CI, artifact verification, hardware records, and honest open gates are part of the deliverable.
