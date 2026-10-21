# GPU lane — M3 gate: userspace page flips through the standard DRM ABI

**Status:** design, not started. **Date:** 2026-10-21.
**Depends on:** IFTU/DRM M2 (hardware-passed 2026-09-01), `vita-linux-next` `c8540dc`.
**Explicitly not:** SGX. This gate touches no secure world, no SMC, no firmware.

## Why this gate, and why now

`docs/GPU-DISPLAY.md` says the next display step is not another GPU-enablement attempt — it is
connecting the already-proven flip path to the normal DRM atomic/page-flip userspace ABI. The
evidence that this is the right next step:

- The M2 ladder **passed on hardware** (Gates A–D, 2026-09-01). vblank IRQ is real, the inactive
  plane-config programs correctly, and repeated page flips ran.
- The code still exists: `topic/iftu-m2` @ `c21ecbaa`, **11 commits** on `ed91f5ce` (`vita-pstv`),
  carrying the M1 driver from `topic/iftu-drm` @ `707317ba`.
- It is not lost work, and it is not far away: both branches are the **same kernel version**
  (6.12.0), and the current integration tip is **26 commits ahead** of the M2 base with a clean
  merge base at `ed91f5ce`. The drift is ordinary feature work, not an API migration.
- What M2 proved was a *driver-internal* flip. What no gate has yet proved is that an ordinary
  Linux program can drive scanout through `/dev/dri` with dumb buffers and page-flip events —
  which is the thing that makes the display useful to software that wasn't written for this port.

So the claim this gate exists to establish, or refute, is narrow and checkable:

> A stock userspace program, using only the standard DRM ioctl ABI, can present frames on the
> PSTV screen at the panel's fixed rate, repeatedly, without wedging the display.

## Inputs

| item | value |
|---|---|
| kernel base | `vita-linux-next` tip `37b93487…` (6.12.0) |
| driver source | `topic/iftu-m2` (`c21ecbaa`), 11 commits |
| merge base | `ed91f5ce13` (`vita-pstv`) |
| config | `CONFIG_DRM=y`, `CONFIG_DRM_VITA=y` added to `vita_defconfig` |
| timing | fixed 1280×720 @ 52.099 Hz — **not** retuned by this gate |
| harness | `tools/hardware-gate.py` for boot-level evidence; a phase probe for the rest |

Verified hardware facts this gate relies on (from the M2 design, all previously hardware-checked):

| what | address |
|---|---|
| plane config selector (**the flip switch**) | `0xE5032000 + 0x10 + plane*0x08` |
| plane config blocks | `+0x200 + cfg*0x100`, `FB_PADDR` at `cfg+0x00` |
| vblank flag (per-frame toggle) | `0xE5031004` |
| scanout byte position (sweeps 0..0x384000) | `0xE5030030` |
| DSI1 intr status / ack | `0xE5060050` / `0xE5060048` |
| vblank IRQ | GIC 210 → `GIC_SPI 178` in DT |

## The verification problem, stated up front

The operator cannot see the screen in the general case, and this project has already been burned
by inferring display state from things that don't prove it. So **every phase below is judged on
signals readable over SSH**, and each one is chosen because it can *fail* — a commit that returns 0
while nothing switched is the exact failure mode this gate must catch.

Three independent signals, none sufficient alone:

1. **DRM events.** `drmModePageFlip` completion callbacks counted in userspace. Proves the kernel
   thinks a flip completed.
2. **Register readback.** Read the plane-config selector and the scanout byte position through
   `/dev/mem` before, during, and after the flip. Proves hardware state actually changed, and that
   scanout is advancing rather than frozen on a stale config.
3. **Cadence.** vblank events per second over a fixed window, expected ≈ 52.1 Hz.

A flip that "succeeds" with (1) but not (2) is a **no-op flip** and is a FAIL, not a pass with a
footnote. That distinction is the whole point of the gate.

## Phases

Each phase has its own stop condition. A phase that fails does not get retried with a tweak in the
same session — it gets recorded, and the next attempt starts from a written hypothesis.

### M3-P0 — rebase, build only (no hardware)

Rebase the 11 commits onto `37b93487`, add `CONFIG_DRM_VITA`, build.

- **PASS:** image builds, driver links, no new warnings in `drivers/gpu/drm/vita/`.
- **FAIL:** conflicts that require redesign, or build breakage not resolvable without changing
  behaviour.
- **Claim:** none. A build is not evidence about a display.

### M3-A — load without taking over scanout

Boot the new kernel. The driver registers, enumerates connector/CRTC, and claims the vblank IRQ —
but **does not** commit a frame.

- **PASS:** `/sys/class/drm/*/status` shows the connector; the vblank IRQ is claimed exactly once
  in `/proc/interrupts` and increments at ~52 Hz; the box boots to SSH normally and scanout is
  untouched (the loader's frame stays on screen).
- **FAIL:** IRQ storm; oops on probe; a hang at any point; SSH never arrives.
- **Why it can fail:** the driver inherits a framebuffer the loader set up. Taking it over at probe
  time is easy to do by accident and hard to see.
- **Corrected 2026-10-21, after the first hardware run:** this criterion originally required the
  HDMI console to survive. It cannot: the M2 DTS series replaced the `simple-framebuffer` node with
  plain reserved memory, so `fbcon` has no framebuffer to attach to on *any* kernel from this
  branch. Console survival was moved to **M3-E**, where the DRM driver is supposed to *become* the
  console. See `lab/gpu-m3-ab-run-2026-10-21.md`.

### M3-B — one flip, from an ordinary program

A self-contained C probe, **raw DRM ioctls, no libdrm dependency**, cross-compiled and statically
linked so nothing has to be installed on the device: read a mode off the connector, `SETCRTC` with a
dumb buffer filled to a solid colour, `ADDFB`, then one page flip with an event, and wait for
completion.

*(Corrected 2026-10-21: the original sequence went straight to `PAGE_FLIP`, which was written
assuming a lit CRTC. On this driver the CRTC reports `enable=0 active=0` — nothing has ever enabled
it — so a flip is rejected before it is attempted. The modeset is not scaffolding around the flip;
on this hardware it is the first userspace commit, and therefore the first thing that exercises
`atomic_update()` at all.)*

**Split in two, because reading the rebased driver changed what this phase is.**

The driver as inherited from M2 does **not** have a userspace page-flip path. The selector toggle
lives only in `vita_iftu_do_page_flip()`, reachable from the debugfs `flip` file, while
`atomic_update()` blits the shadow framebuffer into `idev->screen_base` — the buffer currently
being scanned out. A `drmModePageFlip` against that code would therefore change what is on screen
*without ever touching the selector*, and with the write landing mid-frame. Under the rule below,
a legitimate flip would be recorded as a no-op. The rule was right; the driver was not there yet.

So:

- **M3-B1 — wire the flip (code).** `atomic_update()` blits into the **inactive** buffer and marks
  a flip pending; the **vblank ISR** writes `PLANE_A_SEL`, because the commit path runs in atomic
  context and cannot sleep while the frame boundary is exactly where the switch must land; the
  completion event is armed in `atomic_flush` so userspace is not told the flip finished before
  the frame carrying it has been scanned out. A commit with no damage must not arm a flip, or it
  would present a stale buffer as new.
- **M3-B2 — verify it (hardware).** The userspace probe above, judged by the three signals. This is
  where PASS/FAIL is decided; B1 compiling is not evidence of anything.

M3-B2 passes when: the commit succeeds **and** the selector register changed **and** the flip event
arrived within ~5 vblanks **and** the scanout position is advancing.

- **FAIL:** success with an unchanged selector (no-op flip); event never arrives; scanout frozen.
- **Human cross-check (optional, not required):** the screen changes colour. Recorded if offered,
  never load-bearing.

### M3-C — repeated flips at rate

Alternate two buffers for a fixed window (target: 30 s ≈ 1,560 flips).

- **PASS:** flip completions ≥ 95% of the vblank cadence; selector observed toggling on repeated
  readback; vblank rate stays ≈ 52.1 Hz; no WARN/oops in `dmesg`.
- **FAIL:** completion rate collapses, cadence drifts, selector stops toggling mid-run, or the box
  locks (lock = hard fail, power cycle, record).

### M3-D — lifecycle and cleanup

The part that is always skipped and always the thing that ruins the demo:

1. `kill -9` the flip probe mid-run — the display must not stay wedged.
2. Return to fbcon/VT and confirm the console is usable again.
3. Clean `reboot` from the post-flip state — must come back up.

- **PASS:** all three, with the final boot verified by `tools/hardware-gate.py` writing a normal
  gate record.
- **FAIL:** any wedged display, any boot that requires a power cycle after a *clean* reboot.

### M3-E — the console (last, and only after B2 passes)

`drm_fbdev_shmem_setup()` is deliberately disabled in the driver, and the comment there is explicit
about the condition for re-enabling it: *"Do not re-enable this call until the debugfs flip path is
wired into DRM's atomic page_flip API (userspace drmModePageFlip) and an explicit fbdev-console gate
passes on hardware."* M3-B2 is that wiring proven; M3-E is that gate.

This is the phase that makes the display *usable* rather than merely correct: a console and ordinary
fbdev clients on the DRM device, instead of a fixed buffer nobody pages. It is also the phase that
can lose the screen — today `simplefb` owns `/dev/fb0` and the DRM driver draws nothing, so a
failure here takes the console away and replaces it with nothing.

- **PASS:** fbcon comes up on the DRM device; text is legible; SSH unaffected; a clean shutdown from
  the DRM console returns to a working boot; no wedge.
- **FAIL:** console lost with no replacement, corrupt output, or a boot that needs a power cycle.
- **Rollback:** the phase is a single call to remove; record it before running so the revert is a
  one-line change and not an archaeology exercise.

## Outcome vocabulary

| outcome | meaning |
|---|---|
| **PASS** | all four phases passed on hardware, evidence recorded |
| **PARTIAL** | some phases passed; each recorded separately with the same vocabulary — never merged into a vague "mostly works" |
| **FAIL** | a phase failed and the failure is understood |
| **INCONCLUSIVE** | the run could not distinguish pass from fail (e.g. box wedged before evidence was read) — recorded as such, never as a pass |

## Cost and risk

- **Time:** rebase + build ≈ 30–45 min; each device cycle ≈ 6 min (boot 4–5 min + deploy).
  Expected 4–6 cycles → **an afternoon**, not a week.
- **Mitigations:** every phase is judged over SSH, so the screen being dark is not a blocker.
  Power cycling is available (HA switch) and is the recovery path for any wedge.
- **Real risks:**
  - the driver may grab scanout at probe and kill the console before M3-A can observe it;
  - `/dev/mem` reads may be refused if the kernel ships strict devmem — if so, the register
    readback moves into the driver via debugfs, which is extra work but keeps the same falsifiers;
  - the loader's own IFTU bring-up may leave state the driver expects differently;
  - a wedge costs a power cycle and possibly an ambiguous result.

## Non-goals

Not in this gate, and not to be quietly folded in: SGX/PowerVR, firmware or secure-world calls of
any kind, PLL retuning, EDID or modes beyond the existing fixed timing, scaling/CSC, a second
plane, DSI/ADV7533 modeset, or acceleration of any kind. (fbdev console takeover is M3-E, gated
behind B2 — it must not creep into the B or C phases.) This gate is about *presentation through a
standard interface*. `docs/GPU-DISPLAY.md` keeps acceleration parked until there is new
secure-world provenance, and nothing here changes that.

## Definition of done for the lane (not this gate)

This gate makes the display usable by ordinary Linux software. The lane is only "GPU acceleration"
when a userspace stack can submit work to the SGX — a different problem with a different evidence
boundary, and not unlocked by anything here.
