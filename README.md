# Vita Linux Next

An independent continuation of the work to bring Linux to the PlayStation Vita and PlayStation TV.

Vita Linux Next is an experimental ARM/Linux bring-up project: part kernel port, part boot-chain work, part hardware archaeology. The goal is not to pretend that a consumer console is an ordinary SBC. The goal is to make the machine useful, observable, and increasingly complete while preserving the evidence that tells us why each piece works.

> **Status (2026-09-12):** Linux 6.12 boots on the Vita 1000 and PSTV. The project is active research and development, not a finished distribution.
>
> See the [work matrix](docs/PROJECT-STATUS.md) for what is done, in progress, not implemented, or on hold. Every hardware claim is tied to a dated gate rather than inferred from a successful build.

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
- a corrected 165.6675 MHz ARM private/global-timer clock, validated against the independent RTC
- ARM PMU event counting, including the direct measurement that established `PERIPHCLK = CPU/2`

The PSTV additionally has a validated external Type-A USB path:

- automatic Sony USB host-mode selection and VBUS power sequencing
- high-speed USB storage through the EHCI controller
- a production bus-0 OHCI companion driver for low/full-speed devices
- full-speed mouse, USB-audio control, and Keychron keyboard enumeration through `pstv-ohci`
- real low-speed enumeration and HID binding with a PixArt Lenovo USB Optical Mouse
- sustained mouse/numpad interrupt traffic and an interactive Keychron HDMI-console login
- EHCI high-speed storage hotplug while the OHCI companion remains resident

The current timer/OHCI kernel integration is under review in [`acgh213/linux_vita#10`](https://github.com/acgh213/linux_vita/pull/10) at the hardware-tested commit `0d1ba53a4376`. The detailed evidence is recorded in the associated [vita-linux-research](https://github.com/acgh213/vita-linux-research) repository and the [September 12 multi-device gate](lab/usb-production-multidevice-2026-09-12.md).

The [Vita Linux Workbench](docs/WORKBENCH.md) adds a verified, immutable SquashFS toolkit and an explicitly writable USB workspace for persistent source, builds, logs, and captures. The base system remains a RAM-backed rescue initramfs and does not require USB storage to boot.

## What is still experimental

- Production OHCI is enabled at boot on PSTV and has passed low- and full-speed device gates, but several teardown/reboot/cold-boot lifecycle cases remain untested.
- The Vita handheld's external USB behavior still needs separate, careful characterization.
- SD2Vita and the proprietary Vita memory-card path are not production-ready.
- Display support is currently framebuffer/IF-TU focused; GPU/SGX access remains a separate research lane.
- The persistent Workbench exists and has passed its core PSTV gates; its rebuilt dashboard and reboot-persistence gates remain open.

Negative results are part of the project. A failed gate is recorded and explained rather than silently converted into a success story. The current sequencing lives in the [hardware roadmap](docs/HARDWARE-ROADMAP.md); [GPU and display paths](docs/GPU-DISPLAY.md) explains why standard DRM/KMS progress is real without calling it SGX acceleration.

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

# Run the repository-owned host gates:
make test CART_CROSS_CC=arm-linux-gnueabihf-gcc

# First rootfs build, or after rootfs configuration changes:
make rootfs LINUX_VITA_DIR="$LINUX_VITA_DIR"

# Apply the Vita defconfig, build the kernel, and verify Vita DTBs:
make config LINUX_VITA_DIR="$LINUX_VITA_DIR" CROSS_COMPILE="$CROSS_COMPILE"
make build LINUX_VITA_DIR="$LINUX_VITA_DIR" CROSS_COMPILE="$CROSS_COMPILE"
make verify-dtb LINUX_VITA_DIR="$LINUX_VITA_DIR" CROSS_COMPILE="$CROSS_COMPILE"
```

`make dtb` is also a standalone entrypoint: when needed it generates the Vita kernel configuration and builds the kernel's `dtc` host tool before producing and structurally verifying all three model DTBs.

See [`BUILDING.md`](BUILDING.md) for the longer build notes and [`WORKFLOW.md`](WORKFLOW.md) for the development sequence. A deploy requires a VitaOS-side loader, a reachable target, and explicit artifact/hash verification; it is not part of an ordinary build.

Contributions are welcome. [`CONTRIBUTING.md`](CONTRIBUTING.md) explains the repository split, test gates, hardware-evidence standard, and safety boundaries.

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

Original code and documentation in this outer repository are available under the [MIT License](LICENSE). Submodules, imported packages, firmware, generated sysroots, and other third-party components retain their own licenses; see those components for their authoritative terms.
