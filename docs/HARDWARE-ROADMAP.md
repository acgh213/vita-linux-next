# Vita/PSTV hardware roadmap

**Evidence snapshot:** 2026-09-12

**Kernel integration:** `acgh213/linux_vita:vita-linux-next` at `0d1ba53a4376`

This roadmap ranks work by information gained, safety, and how much capability it unlocks. It does not promote a desktop build into a hardware result.

## Protected baseline

Do not regress these real-hardware gates:

- four Cortex-A9 cores online;
- HDMI framebuffer console and interactive login;
- RTC plus clean reboot/poweroff;
- read-only eMMC and explicit removable-storage discipline;
- Wi-Fi, Bluetooth, and PSTV internal Ethernet;
- EHCI high-speed storage and hotplug;
- production OHCI full-speed HID traffic and low-speed HID enumeration/binding;
- corrected 165.6675 MHz timer rate;
- ARM PMU hardware-event counting;
- deterministic SquashFS toolkit, explicit RW Workbench workspace, native TinyCC/sysroot, and curated examples;
- old-rootfs game-card boot with journalled workspace, power-cycle persistence, and disposable journal-replay proof.

## Priority 1 — finish production OHCI lifecycle acceptance

The data path is no longer the question. Remaining gates are about ownership and teardown:

1. unbind/teardown while a child device remains attached;
2. observed reboot ordering against the VBUS notifier;
3. repeated warm boots;
4. one true cold/power-off boot;
5. explicit system-suspend rejection.

A real 1.5 Mb/s device is **complete**: `17ef:608d` PixArt Lenovo USB Optical Mouse enumerated through `pstv-ohci` and bound `hid-generic`. See [`../lab/usb-production-multidevice-2026-09-12.md`](../lab/usb-production-multidevice-2026-09-12.md).

## Priority 2 — recover the bootable toolchain Workbench

The old-rootfs game-card system is usable and persistent on PSTV hardware. A
4 GiB journalled workspace image survived a power cycle, and a disposable dirty
image mounted after ext4 journal replay with zero corrupt recovered files. The
explicit USB session/build/example gates are also recorded. Remaining work is
now bounded by the newer rootfs candidate:

1. diagnose the unbooted 91 MiB full toolchain image (`make`, `git`, `python3`,
   `opkg`, `e2fsprogs`) using the loader's HDMI progress output;
2. keep the 64 MiB trimmed image (without `git`/`python3`) marked built-only until
   it is deployed and boot-proven;
3. once a candidate boots, rebuild/upload the fixed dashboard payload and run the
   bounded dashboard gate with one supervised physical input;
4. repeat the persistence gate against that exact image and record expected loss
   of RAM-initramfs state, including Bluetooth pairing under `/var/lib/bluetooth`.

This unlocks sustained on-device utilities and experiments without turning the rescue rootfs writable.

## Priority 3 — USB audio

Production OHCI now enumerates a Kingston HyperX Amp. Its HID consumer-control interface binds; streaming audio is not yet proven.

Next bounded gate:

1. confirm the kernel configuration includes USB Audio Class support;
2. enumerate interfaces and ALSA devices without starting playback;
3. attempt a short, bounded playback to the external dongle;
4. inspect underruns, faults, disconnect cleanup, and EHCI coexistence.

This is cheaper and safer than resuming the incomplete Vita codec path, and it delivers useful audio without solving the proprietary internal audio stack first.

## Priority 4 — PMU integration and CP14 hardware breakpoints

Two adjacent but distinct lanes:

- **PMU:** counting works through CP15. Move the minimal `/pmu` node into `pstv.dts`; decide whether IRQ-backed sampling is valuable enough to wire.
- **Hardware breakpoints:** `arch_hw_breakpoint_init()` dark-boots because secure world gates CP14 debug access. Bisect the exact faulting access with the existing boot-safe configuration ladder, then design a safe-fail path. Do not treat CP15 PMU success as evidence that CP14 is available.

## Priority 5 — normal DRM/KMS userspace ABI

The IFTU/DRM M2 ladder already proves mode enumeration, vblank IRQs, inactive-plane programming, and repeated page flips. The missing step is to expose that tested path through the ordinary DRM atomic/page-flip ABI and exercise it from userspace.

This can replace ad-hoc framebuffer ownership with a standard display interface. It does not require SGX and does not imply 3D acceleration.

## Priority 6 — device-specific storage and handheld work

### Vita-handheld external USB

PSTV Type-A host results do not transfer automatically. Characterize role selection, power, connect/disconnect behavior, and controller ownership separately on Vita 1000/2000.

### SD2Vita and Vita memory card

Both remain read-only-first research lanes. Before any mutation:

- identify the exact block device and controller path;
- collect partition/filesystem metadata read-only;
- image unique media before repair or write experiments;
- prove disconnect and reboot behavior;
- retain a known-good physical rollback.

### Battery/power ABI

No production power-supply driver exists. This matters primarily to handhelds, so it follows the PSTV-enabling work above unless new evidence makes it a prerequisite.

## Priority 7 — distribution polish

- consumer-ready image and release artifacts;
- network control/mutation service with an explicit trust boundary;
- broader package catalog on the Workbench workspace;
- release notes, hashes, rollback instructions, and model-specific install guidance.

## Deliberately on hold — SGX

The SGX543MP4+ acceleration lane remains on hold after secure-RAZ behavior, KBL decryption limits, SMC `0x107` denial, and the VDDG revision-gate refusal. No guessed secure SMC calls. Resume only when a provenance-backed secure-world path or another reversible lever appears.
