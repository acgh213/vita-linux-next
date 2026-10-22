# M3 continuation: cadence corrected, sustained rate FAIL, recovery works, DRM console restored

## Outcome by gate

- **Timing description: corrected and deployed.** The already-fixed A9 timer is unchanged.
  A read-only 60-second sample counted 3600 IRQs over 60.058385807 s, approximately **59.94 Hz**.
  DRM's stale 64473 kHz mode description was replaced with the inherited nominal 74176 kHz,
  rounded from the loader's 74.1758 MHz. No hardware clocks, timing registers or timer DT were changed.
- **M3-C: FAIL on throughput.** 600 validated alternating-buffer events over 30.030448181 s,
  **19.9797 flips/s**, below 95% of 59.94 Hz. The independent saved-log evaluator found cadence
  to be the sole recorded failure: sampled selector parity, copy/presentation counts, event
  identity/sequence and continuing scanout were consistent. No new dmesg lines during that run.
- **M3-D: scoped display lifecycle checks passed.** Normal exit disabled the CRTC; SIGKILL during
  active flipping also released it, leaving no pending flip. A new client subsequently modeset
  and flipped successfully. A requested orderly reboot returned to VitaOS and the same Linux
  image relaunched without a power cycle. This is not a storage-durability certification.
- **M3-E: Linux fbcon portion and reboot persistence passed; physical HDMI confirmation open.**
  With explicit `vita_iftu.fbdev=1`, `/proc/fb` lists `0 vita-iftudrmfb`, `/dev/fb0` exists,
  fbcon switches to 160x45 characters at 1280x720, and rendered console text is legible in a
  framebuffer readback. The marker is transcribed below. Another orderly reboot and single
  loader relaunch restored the same DRM framebuffer console without a power cycle. We have no
  independent HDMI capture or human monitor observation, so do not call physical text legibility
  or tear-free HDMI output proven.

**Overall: PARTIAL, not full M3 PASS.** Throughput remains a real defect relative to the near-refresh
criterion. Console usability and recovery are separate results, not a way to erase that failure.

## What the timing counters establish

The 30-second trial used two separately allocated, nearly identical grey frames (RGB 32/32/32
and 33/33/33), not a repeating grey/magenta strobe. Every 61 flips, plus the first flip, a hardware
state snapshot was taken. The source and mock/control-flow tests are in umbrella commit `e3a6648`.

Relative to the post-modeset snapshot, the driver recorded 600 copies and 600 presentations:

- Mean full-frame `drm_fb_blit` plus producer `wmb`: **34.131484055 ms**.
- Mean publication-to-selector-IRQ wait: **14.951866760 ms**.
- Mean userspace ioctl duration: **0.478720742 ms**.
- Mean userspace post-ioctl event wait: **49.396232873 ms**.
- Raw hardware vblank delta across the repeated flips: **1800**, consistent with three refresh
  periods per presented frame. Display cadence is not 20 Hz; presentation throughput is.

The ioctl returns before the deferred atomic tail does most of the work. Therefore its short
runtime must not be mistaken for a cheap CPU copy: the driver counters locate the copy inside
what userspace sees as event wait. The current probe uses legacy XRGB8888; the scanout format is
ABGR8888, so the blit includes conversion. Separating conversion cost from native-format copying
is the next performance experiment, not a claim that format conversion accounts for all 34 ms.

Kernel wall-time measurements include preemption/contention; this is observed blit latency,
not a CPU-cycle attribution. Sampling and tracing add overhead. IRQ sampling bounds in
`cadence-60s.json` bracket read times only; integer IRQ phase and timer-model uncertainty also
apply. They do not prove an exact oscillator frequency.

## Inputs and provenance

Cadence/instrumentation kernel: `711b46d1f5a8`, boot release `6.12.0-g711b46d1f5a8`.

- zImage: 32,193,224 bytes;
  SHA-256 `3370abd647ae2b3f3cbc4bce385bf0c3df4067ba6a26e7c49ce1a70b36088482`.
- Standard DTB: 6,388 bytes;
  SHA-256 `89b1dd57e9f7fd5c6ff71deb86d7ca3d67a7e9394ff08ddccaec164df1593ebe`.
- Static repeated-flip probe: SHA-256
  `a333884469f6475ee3dcde40b04ac62110bc7bcb74a8f6d379b0317c8ab7b104`;
  hash verified remotely before use.

Opt-in console kernel: `81e636543b351d4f2fbf07f925564dd49be935c0`, boot release
`6.12.0-g81e636543b35`. The fbdev module parameter defaults false; no production gitlink changed.

- zImage: 32,192,152 bytes;
  SHA-256 `6a35e81286a5a8fc696a094074a6b6da3391eeb246b003ffb91636afe98dcd5f`.
- Gate DTB: 6,408 bytes;
  SHA-256 `1ff66ae21c994ed02d022f13967113ea081fa73d86fd9ce0f2cc5e99adce95c3`.
  The decoded-tree diff proves the **only** DT change is appending `vita_iftu.fbdev=1` to chosen
  bootargs. No geometry/timer/clock properties changed. Reproduce by copying the standard DTB
  and `fdtput -t s <copy> /chosen bootargs 'earlyprintk keep_bootcon console=ttyS0,115200 console=tty0 vita_iftu.fbdev=1'`.
- Unchanged loader `435791f`: 105,928 bytes;
  SHA-256 `f5712184208467d56cc6626a9d3a17118057e126b34e63113a4f50ff8b0204ab`.

Before each upload the live payload/kernel/DTB were preserved locally under ignored
`rollback-artifacts/` and as hash-suffixed device siblings, readback-hashed before deployment.
New deployed inputs were readback-verified. Each launch used one destroy, 30 seconds settle,
one launch. The cadence boot used a power cycle after halt; all later clean-reboot gates used
VitaOS relaunch **without** a power cycle. Boot-tool timing values are post-takeover SSH waits,
not total latency.

## Raw evidence map

- `gates/2026-10-21-m3-corrected-boot/cadence-60s.json`: read-only cadence sample.
- `gates/2026-10-21-m3-cadence-boot/`: cadence kernel boot + initial state.
  - `sustained-30s/`: transcript, remote exit 1, pre/post dmesg, reproducible FAIL verdict.
  - `kill-active/`: exact probe PID, SIGKILL exit 137, pre/post state; no userspace cleanup ran.
  - `post-kill-one-flip/`: fresh client exit 0, new modeset/flip, normal cleanup.
  - `clean-reboot-ports.json`: return to VitaOS after reboot request.
- `gates/2026-10-21-m3-clean-reboot/`: same cadence image relaunch, boot-tool PASS.
- `gates/2026-10-21-m3-fbdev-boot/`: opt-in console boot, kernel/DTB input hashes and DT diff.
  - `console-initial.txt`: fbcon binding, geometry, matching selector/cache, no pending flip.
  - `console-framebuffer.png`: **fbdev shadow-buffer readback, not HDMI capture**.
  - `fb-var.json`: channel offsets/geometry used to decode that capture.
  - `console-marker.txt`: active VT and readback after text output.
- `gates/2026-10-21-m3-fbdev-reboot/`: reboot/relaunch PASS and independent confirmation that
  `vita_iftu.fbdev=1` is active, `vita-iftudrmfb` returned, and no flip is pending.

The captured marker reads:

```
M3-E DRM CONSOLE CHECK - 2026-10-21
Fixed timer; inherited 59.94 Hz mode; Linux DRM fbcon.
```

The screenshot shows legible, uncorrupted text in the shadow framebuffer. It also retains real
service-startup output, including an nftables startup failure; this is not a claim that every
Debian service is healthy. Boot logs retain the pre-existing inconsistent CPU-mode warning and
other platform messages. Earlier boot logs also contain an exFAT unclean-unmount warning;
requested orderly reboots in this work do not settle the separate shutdown-durability lane.

## Reproduction and remaining work

- Host: `make test CART_CROSS_CC=arm-linux-gnueabihf-gcc`. The five sustained-parser tests run
  everywhere; GNU-linker mock probe tests (18) run on Linux. Test fixtures are not hardware logs.
- On fresh no-fbdev boot: `timeout 50 /tmp/drm-flip-probe --seconds 30`.
  Evaluate saved output with `python3 tools/check-drm-sustained.py <log>`; verify remote exit,
  kernel identity and uploaded hash separately. The known 20 FPS result must remain FAIL.
- Do not run the raw probe on the fbcon-owned active CRTC: it deliberately refuses to steal it.
- Optimize only after a native-format versus conversion comparison on the same hardware and
  memory mapping; no GPU or timer changes are justified by this bottleneck.
- Keep draft kernel PR #13 unmerged and production pin unchanged while throughput and visual
  output confirmation remain outstanding. Disabling the fbdev opt-in requires restoring the
  standard DTB; restoring the prior kernel is also preserved as a rollback path.
