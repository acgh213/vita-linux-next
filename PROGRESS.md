# Vita Linux Next — progress snapshot

**Updated:** 2026-09-12

**Canonical project:** [acgh213/vita-linux-next](https://github.com/acgh213/vita-linux-next)

**Kernel integration branch:** `acgh213/linux_vita:vita-linux-next`

**Hardware-tested kernel:** `0d1ba53a4376`

The detailed, evidence-ranked ledger is [`docs/PROJECT-STATUS.md`](docs/PROJECT-STATUS.md). This file is the short front-page snapshot.

## Current integrated baseline

Kernel `0d1ba53a4376` combines the work already proven on PSTV hardware:

- corrected 165.6675 MHz ARM timer clock;
- production `pstv-ohci` companion driver;
- PSTV device-tree enablement;
- `vita_defconfig` enablement;
- runtime-PM handling needed for production probe.

The kernel series is under review in [`acgh213/linux_vita#10`](https://github.com/acgh213/linux_vita/pull/10). New kernel topic branches should start from `vita-linux-next`, not from the historical manual-gate branch.

## Recent hardware milestones

- `PERIPHCLK = CPU/2` measured directly with PMCCNTR versus the architectural global timer (72 ppm error); the corrected DT rate passed a 300-second RTC gate.
- ARM PMU hardware-event counting works. CP15 PMU access is available; CP14 hardware-debug access remains secure-world gated.
- Production OHCI carries real full-speed HID interrupt traffic and coexists with EHCI high-speed storage/hotplug.
- A Keychron C3 Pro provided an interactive HDMI-console login through production OHCI.
- A PixArt Lenovo USB Optical Mouse passed real 1.5 Mb/s low-speed enumeration and `hid-generic` binding.
- A HyperX Amp USB audio dongle reaches OHCI; only its HID control interface is proven so far, not audio playback.

See [`lab/usb-production-multidevice-2026-09-12.md`](lab/usb-production-multidevice-2026-09-12.md) for the concise multi-device record.

## Persistent userspace

The Vita Linux Workbench is implemented through its core PSTV gates:

- RAM-backed rescue initramfs remains independent of removable storage;
- deterministic SquashFS toolkit mounts read-only;
- an explicit RW USB workspace holds source, builds, logs, and captures;
- session activation verifies hashes and tears down in reverse order;
- pinned TinyCC plus the matching glibc sysroot compiles multi-file C with pthreads and `libm` on-device;
- curated native examples and the status tooling have hardware passes.

Still open: rebuild and rerun the fixed framebuffer dashboard payload, then prove workspace persistence across reboot. Workbench implementation is tracked in repository PR #1.

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
