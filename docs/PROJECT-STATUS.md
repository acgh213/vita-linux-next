# Vita Linux Next — work matrix

**Status snapshot:** 2026-09-12
**Canonical home:** [acgh213/vita-linux-next](https://github.com/acgh213/vita-linux-next)

This is the project ledger for the standalone repository. It separates work that
has actually run on hardware from work that only has a build, static analysis, or
research result.

## State legend

- **DONE** — implemented and backed by a recorded host or hardware result.
- **IN PROGRESS** — the next implementation or gate is defined and active.
- **NOT IMPLEMENTED** — intentionally not present yet; no result is implied.
- **ON HOLD** — investigated enough to stop spending hardware cycles for now.
- **BLOCKED** — a concrete prerequisite prevents the next safe step.

## Work matrix

| Lane | State | Implemented / verified | Next work or gate |
| --- | --- | --- | --- |
| Project home and provenance | **DONE** | Fresh public repository, current README, origins/credits, reproducible repo map, inherited ledger preserved under `docs/history/`. | Keep this matrix current as lanes move. |
| Boot chain and loader | **DONE** | Vita 1000 and PSTV boot Linux 6.12 through the baremetal loader; loader/VitaOS handoff, artifact hashing, rollback, and settle-delay rules are recorded. | Keep deploy artifacts exact-tip and hash-verified. |
| SMP | **DONE** | Four CPUs online on PSTV; CPU0 stack/mailbox collision root-caused and fixed by the production payload stack reservation. | Upstream/loader submission remains deferred; do not reopen the fixed lane without new evidence. |
| Syscon transport | **DONE** | Busy/status semantics, validated reads, Baryon probe, RTC, Wi-Fi power sequencing, and K1/K2 verification are merged and hardware-tested. | Future battery/power work must use a reviewed ABI and the handheld gate. |
| Wi-Fi, Bluetooth, Ethernet | **DONE** | SD8787 Wi-Fi, Bluetooth, and PSTV internal Realtek Ethernet are working; the SDIO fn3 AMP firmware race is suppressed by DT/driver policy. | Preserve the known-good firmware/config chain during rootfs rebuilds. |
| Input | **DONE** | Identity-first input inventory and bounded read-only watcher; buttons/touch lanes validated without assuming event numbering. | Add richer physical-event fixtures only if a new consumer needs them. |
| eMMC and storage | **DONE** | eMMC exposed read-only; removable storage discovery and safe mount discipline documented; no implicit writes. | Keep every new storage experiment read-only first. |
| PSTV EHCI host | **DONE** | USB bus 0 host-mode choreography works; EHCI high-speed storage enumerates and hotplugs cleanly while the OHCI companion is resident. | Preserve the EHCI-first admission rule for all companion work. |
| PSTV OHCI diagnostic | **DONE** | Empty-bus read/reset/first-frame gate passed on real PSTV silicon with SOF IRQs, cleanup, quarantine, and `-EBUSY` admission control. | Retain as a bounded diagnostic path; production operation no longer depends on manually loading it. |
| OHCI HCD and HID | **DONE / LIFECYCLE PENDING** | Production `pstv-ohci` is enabled from boot at kernel candidate `0d1ba53a4376`. Full-speed composite HID carried 9,848 real mouse/numpad interrupts; a Keychron C3 Pro provided an HDMI-console login; a PixArt Lenovo mouse passed real 1.5 Mb/s low-speed enumeration and `hid-generic` binding. EHCI coexistence and hotplug remained clean. | Finish attached-child teardown, reboot/VBUS-notifier ordering, repeated and cold boots, and system-suspend rejection. Kernel review: [`acgh213/linux_vita#10`](https://github.com/acgh213/linux_vita/pull/10). |
| Timer clock provenance | **DONE** | Direct PMCCNTR:GT measurement established `PERIPHCLK = CPU/2` at 72 ppm; the corrected 165.6675 MHz DT rate passed a 300-second independent RTC gate. | Preserve the measured rate; do not reopen the inherited 144 MHz value without contradictory hardware evidence. |
| ARM PMU counting | **DONE / INTEGRATION PENDING** | CP15 PMU access is safe and hardware events count. | Move the minimal `/pmu` node into `pstv.dts`; decide whether IRQ-backed sampling is worth wiring. |
| Hardware breakpoints | **BLOCKED** | `arch_hw_breakpoint_init()` reaches secure-world-gated CP14 debug registers and dark-boots. | Bisect the exact faulting access with the boot-safe ladder before proposing a safe-fail path. |
| Simple framebuffer | **DONE** | PSTV 1280×720 RGBA geometry, safe capture/restore, fbcon ownership, and framebuffer-serving tooling are validated. | Keep writes explicit and shadow-rendered. |
| IFTU/DRM display | **DONE** | Mapping bug fixed; mode enumeration, vblank IRQ, inactive-plane programming, one user-confirmed flip, and repeated page flips all passed the M2 ladder. | Wire the tested flip path into normal DRM atomic/page-flip userspace ABI. |
| SGX/GPU | **ON HOLD** | Register/gate surveys, secure-RAZ behavior, KBL decryption, SMC 0x107 denial, and VDDG revision-gate refusal are documented. | No more guessed secure SMCs; resume only with secure-world provenance or a new safe lever. |
| Audio | **ON HOLD / NEW TEST PATH** | No supported Vita audio driver or hardware playback gate. A HyperX Amp now enumerates through production OHCI, but only its HID control interface is proven. | Enable and test USB Audio Class streaming before returning to Vita codec work. |
| Rootfs and Buildroot | **IN PROGRESS** | The old-rootfs game-card baseline boots to a usable persistent PSTV system; its journalled workspace and crash-replay behavior are proven. The current `a63020d`/`78917a1` full toolchain image packed successfully but did not boot after upload. | Diagnose the unbooted full image from the loader's HDMI progress output; the 64 MiB trimmed image is built but not deployed or boot-proven. Keep the known-good rollback separate. |
| Native toolkit | **DONE** | `vita-diag`, `vita-netdiag`, `vita-storage`, `vita-fb`, `vita-fbserve`, `vita-usbinfo`, input tools, control, and native C development flows have host tests and hardware validation records. The native SquashFS is now mounted read-only on the PSTV; diagnostics, two-file pthread/`libm` linking, and a 320×180 read-only framebuffer HTTP snapshot passed. | Add new commands only with machine output, bounded behavior, and a host fake-root test. |
| Vita Linux Workbench | **IN PROGRESS** | The separate 2026-09-12 game-card gate proves a usable PSTV login with a hash-verified read-only toolkit and a persistent 4 GiB journalled workspace; a marker and compiled program survived a power cycle, and a disposable dirty-image test produced successful ext4 journal replay with zero corrupt recovered files. The earlier USB Workbench session/build/example gates and dashboard refusal contract also remain recorded. | Do not upgrade the current full toolchain rootfs to booted status: its 91 MiB image failed to return after upload. The 64 MiB trimmed image is not deployed. Rebuild/re-upload a bootable candidate, then re-run the dashboard and persistence gates against that exact image. Evidence: [`lab/usable-system-2026-09-12.md`](../lab/usable-system-2026-09-12.md). |
| Native ARM compiler | **DONE** | Pinned TinyCC plus matching glibc development sysroot works on PSTV for multi-file C, linking, pthreads, `libm`, and bounded project tests. | Keep static GCC cross-builds as the production-artifact path. |
| Demo cart | **DONE** | Framebuffer canvas/scenes, worker pool, input/control, framebuffer ownership, runtime, and temporary candidate lifecycle have passed the recorded gates. | Promote only after a fresh artifact/provenance gate; do not overwrite the known-good rollback. |
| External toolkit payload | **DONE** | Deterministic SquashFS payload builder, manifest, USB staging, read-only loop mount, native tools, and cleanup were validated. | Keep payload and embedded rescue image as separate layers. |
| HyFetch | **DONE** | Buildroot package, ARM Python 3/readline runtime, neofetch backend, and preseeded transgender config are in the embedded image; target execution and pseudo-TTY ANSI colour output passed on the PSTV. | Keep the package pinned and rerun the bounded smoke gate when the base image or Python version changes. |
| Research and reverse engineering | **DONE / ONGOING** | Hardware maps, loader behavior, secure-world/KBL decryption, USB/DRM/SGX evidence, and dated lab records live in the associated research archive. | New claims require a dated record, exact artifacts, and an evidence boundary. |

## Explicitly not shipped yet

- Completion of the production OHCI lifecycle matrix and upstream review/merge.
- A production DRM atomic page-flip implementation wired to normal userspace.
- An SGX/OpenPVR driver or accelerated 3D path.
- A supported audio playback path.
- A battery power-supply ABI/driver.
- A network mutation/control service.
- A consumer-ready distribution image.

## 2026-09-12 evidence boundary

The game-card usable-system result and journal/replay proof were run with Linux
`6.12.0-g321732d0fde7` and rootfs commit `168082d`. They are not evidence that
the current full toolchain rootfs boots. The current branch tip is `78917a1`;
its full 91 MiB image was size-verified after upload but did not return to the
network, and its 64 MiB trimmed alternative has not been deployed. The concise
record is [`lab/usable-system-2026-09-12.md`](../lab/usable-system-2026-09-12.md).

## Evidence rules

A desktop compile is not a hardware pass. Every device lane needs exact source,
configuration, DTB, rootfs, loader/payload hashes, a bounded gate, fault scan,
and a clean rollback to VitaOS or the known-good Linux baseline. If a result is
only static or host-side, this matrix says so instead of upgrading it to DONE.
