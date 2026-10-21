# Handoff — GPU lane, M3 userspace flip gate (2026-10-21)

Phase-end note. The gate is **scoped and merged**; nothing has been run on hardware.
This file exists so the next session starts from evidence instead of re-deriving it.

## Where things stand

| item | state |
|---|---|
| `vita-linux-next` `main` | `28d60d1`, CI green, loader pinned `435791f` (bootable) |
| GPU gate scope | merged as PR #44 → `docs/plans/2026-10-21-gpu-m3-userspace-flip-gate.md` |
| M2 driver code | `topic/iftu-m2` @ `c21ecbaa` in `acgh213/linux_vita` (11 commits) |
| M2 hardware result | passed Gates A–D on 2026-09-01; records in `vita-linux-research/lab/gpu-re/` |
| Current kernel tip | `37b9348710df`, 6.12.0 — **same version** as the M2 base |
| Drift | M2 is 11 behind / 26 ahead of `vita-pstv` `ed91f5ce`; merge base clean |
| SGX | still parked, unchanged. Nothing here touches secure world. |

## The claim the gate must establish

> A stock userspace program, using only the standard DRM ioctl ABI, can present frames on
> the PSTV screen at the panel's fixed rate, repeatedly, without wedging the display.

Not acceleration. Presentation. The lane only becomes "GPU acceleration" when userspace can
submit work to the SGX — a different problem with a different evidence boundary.

## Phases (details in the plan)

- **M3-P0** rebase 11 commits onto `37b93487`, `CONFIG_DRM_VITA=y`, build. No behaviour claim.
- **M3-A** load without taking over scanout. Console must survive; vblank IRQ claimed once at ~52 Hz.
- **M3-B** one flip from a raw-ioctl userspace probe (no libdrm). Success **plus** selector
  register changed **plus** flip event within ~5 vblanks **plus** scanout advancing.
- **M3-C** 30 s of repeated flips; ≥95% of cadence; selector toggling; no WARN.
- **M3-D** `kill -9` mid-flip, fbcon return, clean reboot from the post-flip state.

Outcomes: PASS / PARTIAL / FAIL / **INCONCLUSIVE**. Never round INCONCLUSIVE up.

## Non-negotiable judgement rules

1. **A flip that returns success while the plane-config selector never changes is a FAIL**,
   not a pass with a footnote. A no-op flip is the failure mode the gate exists to catch.
2. Every phase is judged on signals readable **over SSH**. The operator cannot see the screen;
   a dark screen is not evidence either way.
3. Three independent signals, none sufficient alone: DRM events, register readback, cadence.
4. A wedge costs a power cycle and is recorded as such. Recover with the HA switch
   (`switch.switch` — never `switch.washer`).

## Entry points

- Gate design: `docs/plans/2026-10-21-gpu-m3-userspace-flip-gate.md`
- Kernel branch to rebase: `git fetch origin topic/iftu-m2` in `linux_vita/`
- Driver: `drivers/gpu/drm/vita/vita_iftu.c` (M1 tested at `707317ba`)
- Register map and IRQ numbers: the "Hardware contract" table in the plan, sourced from
  `DRM-M2-DESIGN-2026-09-01.md`
- Boot/evidence harness: `tools/hardware-gate.py --payload … --zimage … --dtb … --loader-ref …`
- Device: PSTV `192.168.18.43`; Linux boot baseline 240–330 s

## Known risks, in order of likelihood

1. **Probe-time scanout grab.** The driver inherits the loader's framebuffer; taking it over at
   probe kills the console before M3-A can observe it.
2. **`/dev/mem` refusal.** If strict devmem is on, the register-readback signal has to move into
   the driver via debugfs — same falsifiers, more work.
3. **Loader/driver state disagreement** about the IFTU plane configuration.
4. **A wedge** costing a cycle and possibly an ambiguous result.

## What would make me stop

If two attempts at the same phase fail for *different* reasons, stop and write the hypothesis
down instead of iterating blind. Repeated blind iteration on a display path with no visible
signal is how the earlier "`0x88E` hangs the box" claim got made.

## Resume prompt

> Read `docs/plans/2026-10-21-gpu-m3-userspace-flip-gate.md` in `vita-linux-next` and start at
> **M3-P0**: fetch `topic/iftu-m2` (`c21ecbaa`) from `acgh213/linux_vita`, rebase its 11 commits
> onto `37b9348710df`, add `CONFIG_DRM_VITA=y`, and build. Report the build result only — no
> display claims — then stop before M3-A and tell me what the rebase actually touched.
