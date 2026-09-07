# Vita Linux Next — work matrix

**Status snapshot:** 2026-09-07
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
| PSTV EHCI host | **DONE** | USB bus 0 host-mode choreography works; EHCI FRINDEX advances and external Type-A high-speed storage enumerates and mounts read-only. | Preserve the EHCI-first admission rule for all companion work. |
| PSTV OHCI diagnostic | **DONE** | Empty-bus read/reset/first-frame gate passed on real PSTV silicon with SOF IRQs, cleanup, quarantine, and `-EBUSY` admission control. | This proves the controller window, not enumeration. |
| OHCI HCD and HID | **DONE / INTEGRATION PENDING** | Opt-in HCD gate module has host/backend tests, ARM W=1 build cleanliness, and tested kernel candidate `1ee3edf3c90a`; a direct Keychron C3 Pro enumerated through EHCI → OHCI as `usb3` at 12 Mb/s, bound to `usbhid`/`hid-generic`, created input devices, and reached interactive HDMI console login. Cleanup returned `status=0`, `cleanup_status=0`, `poisoned=0`. | Finish review/merge of [`acgh213/linux_vita#9`](https://github.com/acgh213/linux_vita/pull/9), then promote the tested path into the normal project pin via [`acgh213/vita-linux-port#29`](https://github.com/acgh213/vita-linux-port/pull/29). |
| Simple framebuffer | **DONE** | PSTV 1280×720 RGBA geometry, safe capture/restore, fbcon ownership, and framebuffer-serving tooling are validated. | Keep writes explicit and shadow-rendered. |
| IFTU/DRM display | **DONE** | Mapping bug fixed; mode enumeration, vblank IRQ, inactive-plane programming, one user-confirmed flip, and repeated page flips all passed the M2 ladder. | Wire the tested flip path into normal DRM atomic/page-flip userspace ABI. |
| SGX/GPU | **ON HOLD** | Register/gate surveys, secure-RAZ behavior, KBL decryption, SMC 0x107 denial, and VDDG revision-gate refusal are documented. | No more guessed secure SMCs; resume only with secure-world provenance or a new safe lever. |
| Audio | **ON HOLD** | Static reconnaissance and codec/UDC clues exist; no supported Vita audio driver or hardware playback gate. | Return after display/input/storage provide a stable user-facing toolkit base. |
| Rootfs and Buildroot | **IN PROGRESS** | Reproducible initramfs, signed regulatory database, SSH/Wi-Fi overlays, BusyBox hard-bound tooling, and SquashFS support are established. | Rebuild from the clean canonical tree, verify embedded rootfs provenance, and keep local secrets out of public artifacts. |
| Native toolkit | **DONE** | `vita-diag`, `vita-netdiag`, `vita-storage`, `vita-fb`, `vita-fbserve`, `vita-usbinfo`, input tools, control, and native C development flows have host tests and hardware validation records. The native SquashFS is now mounted read-only on the PSTV; diagnostics, two-file pthread/`libm` linking, and a 320×180 read-only framebuffer HTTP snapshot passed. | Add new commands only with machine output, bounded behavior, and a host fake-root test. |
| Vita Linux Workbench | **IN PROGRESS** | Plan: [`docs/plans/2026-09-07-vita-linux-workbench.md`](plans/2026-09-07-vita-linux-workbench.md). Existing payload activation and native compile primitives are proven; the coherent RW USB workspace, session lifecycle, status command, examples, and dashboard are not yet implemented as one milestone. | Add explicit `--rw-workspace` transport mode, persistent workbench paths, `vita-status`, curated native examples, then the bounded framebuffer/input dashboard and PSTV gate. |
| Native ARM compiler | **DONE** | Pinned TinyCC plus matching glibc development sysroot works on PSTV for multi-file C, linking, pthreads, `libm`, and bounded project tests. | Keep static GCC cross-builds as the production-artifact path. |
| Demo cart | **DONE** | Framebuffer canvas/scenes, worker pool, input/control, framebuffer ownership, runtime, and temporary candidate lifecycle have passed the recorded gates. | Promote only after a fresh artifact/provenance gate; do not overwrite the known-good rollback. |
| External toolkit payload | **DONE** | Deterministic SquashFS payload builder, manifest, USB staging, read-only loop mount, native tools, and cleanup were validated. | Keep payload and embedded rescue image as separate layers. |
| HyFetch | **DONE** | Buildroot package, ARM Python 3/readline runtime, neofetch backend, and preseeded transgender config are in the embedded image; target execution and pseudo-TTY ANSI colour output passed on the PSTV. | Keep the package pinned and rerun the bounded smoke gate when the base image or Python version changes. |
| Research and reverse engineering | **DONE / ONGOING** | Hardware maps, loader behavior, secure-world/KBL decryption, USB/DRM/SGX evidence, and dated lab records live in the associated research archive. | New claims require a dated record, exact artifacts, and an evidence boundary. |

## Explicitly not shipped yet

- Upstream review/merge and default project integration of the tested OHCI/HID path.
- A production DRM atomic page-flip implementation wired to normal userspace.
- An SGX/OpenPVR driver or accelerated 3D path.
- A supported audio playback path.
- A battery power-supply ABI/driver.
- A network mutation/control service.
- A consumer-ready distribution image.

## Evidence rules

A desktop compile is not a hardware pass. Every device lane needs exact source,
configuration, DTB, rootfs, loader/payload hashes, a bounded gate, fault scan,
and a clean rollback to VitaOS or the known-good Linux baseline. If a result is
only static or host-side, this matrix says so instead of upgrading it to DONE.
