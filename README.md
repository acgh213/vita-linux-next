# Vita Linux Next

An independent continuation of the work to bring Linux to the PlayStation Vita and PlayStation TV.

Vita Linux Next is an experimental ARM/Linux bring-up project: part kernel port, part boot-chain work, part hardware archaeology. The goal is not to pretend that a consumer console is an ordinary SBC. The goal is to make the machine useful, observable, and increasingly complete while preserving the evidence that tells us why each piece works.

> **Status:** Linux 6.12 boots on the Vita 1000 and PSTV. The project is active research and development, not a finished distribution.

## What works

Both targets currently have working paths for the core system:

- ARM Cortex-A9 SMP with all four cores online
- framebuffer and console output
- Vita buttons and touchscreen where the hardware provides them
- RTC and clean reboot/poweroff paths
- read-only VitaOS/eMMC storage access
- Wi-Fi through the Marvell SD8787 and Linux networking
- Bluetooth controller support through the shared SD8787 firmware path
- SSH-based remote development and diagnostics

The PSTV additionally has a validated external Type-A USB path:

- automatic Sony USB host-mode selection and VBUS power sequencing
- high-speed USB storage through the EHCI controller
- an opt-in bus-0 OHCI HCD for low/full-speed devices
- direct Keychron C3 Pro keyboard enumeration through the EHCI companion handoff
- `usbhid`/`hid-generic` input registration and an interactive HDMI console

The current OHCI/HID kernel candidate is under review in [`acgh213/linux_vita#9`](https://github.com/acgh213/linux_vita/pull/9). The hardware evidence is recorded in the associated [vita-linux-research](https://github.com/acgh213/vita-linux-research) repository.

## What is still experimental

- OHCI is currently an opt-in/manual bring-up path, not yet a default boot-time HCD.
- The Vita handheld's external USB behavior still needs separate, careful characterization.
- SD2Vita and the proprietary Vita memory-card path are not production-ready.
- Display support is currently framebuffer/IF-TU focused; GPU/SGX access remains a separate research lane.
- Rootfs packaging, hardware probes, and deployment tooling are evolving alongside the kernel.

Negative results are part of the project. A failed gate is recorded and explained rather than silently converted into a success story.

## Repository map

This repository is the outer build and integration project. The major components live in separate repositories:

- [`linux_vita`](https://github.com/acgh213/linux_vita) — Vita Linux kernel fork and kernel-side drivers
- [`vita-baremetal-linux-loader`](https://github.com/acgh213/vita-baremetal-linux-loader) — VitaOS-side loader and Linux handoff
- [`vita-linux-research`](https://github.com/acgh213/vita-linux-research) — reverse-engineering notes, lab records, tools, and hardware evidence
- `buildroot-vita/` — tracked Buildroot external tree, defconfig, overlays, and init scripts
- `cart/` — the PSTV framebuffer/demo-cart and runtime experiments
- `toolkit/` — native Vita-side diagnostics and development tools

The submodules are intentionally explicit. A kernel change, a loader change, and a rootfs change have different failure modes and should remain reviewable as separate pieces.

## Build on Debian/Linux

Initialize the project and submodules:

```sh
git clone --recurse-submodules https://github.com/acgh213/vita-linux-next.git
cd vita-linux-next
```

The Debian development path uses the ARM hard-float cross compiler. The outer Makefile resolves the kernel through `LINUX_VITA_DIR`:

```sh
export LINUX_VITA_DIR="$PWD/linux_vita"
export CROSS_COMPILE=arm-linux-gnueabihf-

# First rootfs build, or after rootfs configuration changes:
make rootfs LINUX_VITA_DIR="$LINUX_VITA_DIR"

# Apply the Vita defconfig, build the kernel, and verify Vita DTBs:
make config LINUX_VITA_DIR="$LINUX_VITA_DIR" CROSS_COMPILE="$CROSS_COMPILE"
make build LINUX_VITA_DIR="$LINUX_VITA_DIR" CROSS_COMPILE="$CROSS_COMPILE"
make verify-dtb LINUX_VITA_DIR="$LINUX_VITA_DIR" CROSS_COMPILE="$CROSS_COMPILE"
```

See [`BUILDING.md`](BUILDING.md) for the longer build notes and [`WORKFLOW.md`](WORKFLOW.md) for the development sequence. A deploy requires a VitaOS-side loader, a reachable target, and explicit artifact/hash verification; it is not part of an ordinary build.

### Local secrets

Wi-Fi credentials and SSH authorized keys belong only in the gitignored `buildroot-vita/board/vita/local/` overlay. Do not put them in commits, issues, pull requests, or lab records. The committed overlay is deliberately non-secret and is applied separately from local credentials.

## Hardware safety

This project writes software to a real console. Keep these rules in view:

- keep VitaOS/eMMC partitions read-only;
- never deploy a zImage without checking its embedded initramfs provenance and SHA-256;
- do not treat a reachable loader port as proof that the loader is ready;
- preserve a known-good rollback image before a hardware gate;
- change one experimental variable at a time where practical;
- use the HDMI console as authoritative evidence when SSH disappears;
- stop on a fault, timeout, checksum mismatch, or unexplained input/storage regression.

## Origins and acknowledgments

This is a fresh project home, not a claim that the work began here. Vita Linux Next continues the public work in [`incognitojam/vita-linux-port`](https://github.com/incognitojam/vita-linux-port), the current fork/archive at [`acgh213/vita-linux-port`](https://github.com/acgh213/vita-linux-port), and the kernel lineage in [`incognitojam/linux_vita`](https://github.com/incognitojam/linux_vita) and [`xerpi/linux_vita`](https://github.com/xerpi/linux_vita).

Thank you to **incognitojam**, **xerpi**, and the contributors represented in those repositories for the original port, hardware knowledge, kernel work, and exploratory groundwork. Their repositories remain linked here as part of the project's history, and the previous fork is preserved rather than rewritten as though it were ours alone.

The current work also stands on the Linux kernel and Buildroot communities, Vita homebrew and reverse-engineering tools, and the people who documented the hardware before this project arrived at it. See [`docs/ORIGINS.md`](docs/ORIGINS.md) for the fuller lineage and contribution boundaries.

## License

The kernel and other components retain the licenses carried by their upstream repositories. Project-specific files should include an appropriate license before they become independently reusable. See individual files and submodules for authoritative licensing terms.
