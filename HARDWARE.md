# Vita/PSTV hardware support

**Status:** 2026-09-12

**Detailed ledger:** [`docs/PROJECT-STATUS.md`](docs/PROJECT-STATUS.md)

**Prioritized work:** [`docs/HARDWARE-ROADMAP.md`](docs/HARDWARE-ROADMAP.md)

This file is a current high-level map. It distinguishes real-hardware results from source reconnaissance and open work. The original February 2026 register reference is preserved as [`docs/history/HARDWARE-2026-02-inherited.md`](docs/history/HARDWARE-2026-02-inherited.md); use the research repository and current kernel source before relying on its older status labels.

## Current integration baseline

The hardware-tested kernel integration is `acgh213/linux_vita:vita-linux-next` at `0d1ba53a4376`.

| Subsystem | Current evidence | Boundary / next gate |
| --- | --- | --- |
| CPU / SMP | All four Cortex-A9 cores online on PSTV and Vita 1000. | Preserve the loader's production stack/mailbox reservation. |
| Timers | ARM global/private timer reference corrected to 165.6675 MHz; `PERIPHCLK = CPU/2` measured at 72 ppm; 300-second RTC gate passed. | Do not restore the inherited 144 MHz value without contradictory hardware evidence. |
| ARM PMU | CP15 hardware-event counting works. | Production `/pmu` DT node and IRQ-backed sampling are not shipped. |
| Hardware breakpoints | CP14 debug access dark-boots during `arch_hw_breakpoint_init()`. | Bisect the exact gated access before designing a safe-fail path. |
| HDMI display | 1280×720 RGBA framebuffer console and interactive login work on PSTV. | Preserve framebuffer ownership/restore rules. |
| IFTU / DRM | Mode enumeration, vblank IRQ, inactive-plane programming, and repeated page flips passed the M2 ladder. | Connect the proven path to normal DRM atomic/page-flip userspace ABI. |
| SGX543MP4+ | Secure/power/register reconnaissance is recorded. | No open acceleration path; on hold until a provenance-backed secure-world or non-secure lever appears. |
| Syscon | Validated transport, RTC, power sequencing, buttons/touch consumers, reboot, and poweroff work. | New child ABIs must fail closed on unknown versions/results. |
| Wi-Fi / Bluetooth | Marvell SD8787 Wi-Fi and Bluetooth work through the shared power/firmware path. | Preserve the disabled fn3 policy and known-good firmware chain. |
| Ethernet | PSTV internal Realtek USB Ethernet works. | Preserve as a separate Sony USB path from the external Type-A port. |
| PSTV EHCI | High-speed Type-A storage and hotplug work. | Preserve EHCI-first admission when changing the companion path. |
| PSTV OHCI | Production driver works from boot; full-speed HID traffic, Keychron console login, real low-speed mouse enumeration/binding, and EHCI coexistence are proven. | Attached-child teardown, reboot ordering, repeated/cold boots, and suspend rejection remain. |
| Vita external USB | No production result inferred from PSTV. | Characterize handheld role selection, power, and connect/disconnect separately. |
| eMMC | Internal eMMC and SCE partitions enumerate; project policy is read-only access. | Keep new storage experiments read-only first. |
| SD2Vita / game card | SDIF1 is known in the driver but disabled in the production DT; a currently inserted adapter creates no block device. Older isolated work reached card-present then hit CRC failures. | Prove adapter/media in VitaOS, then revive only the isolated SDIF1/power candidate and capture MMC errors without mounting. |
| Sony memory card | Proprietary MSIF authentication/data path is not production-supported. | Separate research lane; do not infer it from SD2Vita. |
| Internal audio | Clock/codec/UDC clues exist; no playback gate. | Remains on hold behind cheaper external USB audio. |
| USB audio | A HyperX Amp reaches production OHCI and binds its HID control interface. | Enable ALSA/`snd-usb-audio`, inventory without playback, then run a short bounded external-output gate. |
| Battery / power supply | No production Linux battery ABI. | Read-only Vita-handheld driver first; PSTV must not instantiate it. |
| Persistent userspace | Old-rootfs game-card boot is hardware-proven with a hash-verified read-only toolkit, a journalled workspace, power-cycle persistence, and journal replay with zero corrupt recovered files. | Current 91 MiB full toolchain rootfs did not boot after upload; 64 MiB trimmed image is not deployed. Re-run dashboard/persistence gates only against a booted exact image. |

## Evidence and safety rules

- A desktop compile is not a hardware pass.
- Record exact kernel, DTB, rootfs, loader/payload, and hashes for a hardware claim.
- Keep internal VitaOS/eMMC storage read-only.
- Do not generalize PSTV results to Vita 1000/2000 without a separate gate.
- Display scanout and page flips are not SGX acceleration.
- Do not issue guessed secure-world SMC calls.
- Stop on faults, timeouts, unexplained input/storage regression, or failed cleanup.

Deep register maps, reverse engineering, raw logs, and dated experiments belong in [`acgh213/vita-linux-research`](https://github.com/acgh213/vita-linux-research). Concise integration evidence may also live under [`lab/`](lab/).
