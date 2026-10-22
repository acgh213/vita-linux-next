# Native-format comparison: ABGR sustains refresh rate; XRGB conversion does not

## Result

**M3-C PASS for opaque native ABGR8888, repeated twice. XRGB8888 remains cadence FAIL.**
On the same kernel, four interleaved 30-second trials gave:

| Trial | Source format | Completed flips | Flips/s | Mean blit + WC drain | Mean ready-to-IRQ wait | Cadence gate |
|---|---|---:|---:|---:|---:|---|
| 01 | XRGB8888, converted | 600 | 19.979733 | 34.141244 ms | 14.880353 ms | FAIL |
| 02 | ABGR8888, opaque native | 1799 | 59.939218 | 11.294113 ms | 4.332641 ms | PASS |
| 03 | XRGB8888, converted | 600 | 19.979741 | 34.193528 ms | 14.878338 ms | FAIL |
| 04 | ABGR8888, opaque native | 1799 | 59.939197 | 11.267956 ms | 4.376934 ms | PASS |

Every run used a separately allocated pair of nearly identical grey buffers, identical geometry,
stride and copy destination mapping. Both paths full-copy each frame; neither uses SGX, zero-copy,
hardware acceleration, clock changes or damage-history shortcuts. Native buffers explicitly set
alpha to 255, not zero. No high-contrast repeated strobe was used.

Both native trials passed the independent saved-log evaluator, including matching event cookies,
advancing sequences, sampled selector parity, exact copy/presentation deltas and advancing scanout.
They each advanced 1799 raw hardware vblanks for 1799 presented flips. The XRGB trials advanced
1800 raw vblanks for 600 flips, retaining the earlier three-refresh-period pattern.
All four runs had no new dmesg lines, and independent post-reads showed no pending flip and an
inactive probe-owned CRTC. Remote exits were 1/0/1/0, matching cadence FAIL/PASS/FAIL/PASS.

## What changed before the comparison could run

An initial attempt on kernel `81e636543b35` reproduced XRGB at 19.98 FPS, but native ADDFB2 failed
EINVAL **before modesetting**. That failed attempt is retained under
`gates/2026-10-21-m3-native-format-boot/`.

Source review found that `drm_fb_build_fourcc_list()` replaces alpha-bearing ABGR8888 with XBGR8888.
The driver's advertised list therefore hid its native byte-copy path. The generic `drm_fb_blit()`
has a native exact-format memcpy path and an XRGB-to-ABGR conversion path, but no XBGR-to-ABGR
conversion path. The former list advertised a source format that this destination could not blit.

Kernel correction `2334ad948b1f8d2a395d7c66040d358160f4c821` replaces that generated list with the two
explicitly supported source formats: native ABGR8888 and legacy XRGB8888. It does not modify the
copy functions, selector logic, timing, clock parameters or fbdev opt-in behavior. Both comparison
paths were then run on this **same corrected kernel**, not different kernels for A and B.

## Interpretation and limitations

Across the paired trials the observed XRGB conversion/blit path took approximately three times
as long as the native byte-copy path. That extra latency missed two additional refresh boundaries;
the native path fitted a single refresh period and reached about 59.94 FPS.

These are elapsed kernel durations, not CPU-cycle or memory-bandwidth isolation. Native exact-format
`drm_fb_memcpy()` and XRGB conversion exercise different helper routines; this experiment shows the
cost of those actual paths, not a universal cost for every possible conversion implementation.
Setup differs appropriately (legacy ADDFB versus native ADDFB2); setup is outside the timed flip
loop. Native format uses opaque alpha and grayscale content, so this does not establish arbitrary
alpha blending or non-gray colour correctness on HDMI.

The old XRGB failures are not relabelled as PASS. Ordinary clients can now request the advertised
ABGR8888 format through the standard DRM ABI for the tested full-rate path. Clients requiring the
legacy XRGB conversion still have a measured ~20 FPS limitation. There is no GPU/3D claim, no
physical HDMI capture, and no independent tearing assessment.

## Native-client lifecycle and console restoration

After the four trials, a native client was SIGKILLed during active flipping. It had completed 175
events before death. Kernel cleanup left the logical CRTC inactive and no flip pending; no new
dmesg lines appeared. A fresh native one-shot client then passed the event/selector/scanout check
and performed normal cleanup.

The device was rebooted to VitaOS without power cycling, then relaunched on the same corrected
kernel with the previously verified opt-in DTB. Independent readback confirms:

- `uname -r`: `6.12.0-g2334ad948b1f`.
- `/proc/fb`: `0 vita-iftudrmfb`; fbdev opt-in parameter is `Y`.
- Selector and cached state agree; no flip is pending.
- The fbcon framebuffer is XR24/XRGB8888, so **fbcon still uses the slower conversion path**.
  Restoring the console is not a claim that its clients automatically switched to native ABGR.

The console's physical HDMI appearance still needs independent confirmation. Linux-side console
binding and the previous framebuffer text readback must not be called an HDMI capture.

## Provenance

- Kernel source: `2334ad948b1f8d2a395d7c66040d358160f4c821` (`lab/m3-iftu-m2`).
- zImage: 32,192,232 bytes; SHA-256
  `5de9c77e90ff62ddf52a0cf22f45e68aee21967afb6dbbb728a1b0deb5b9977e`.
- Probe source: umbrella `0fae9c7`; static ARM binary SHA-256
  `c801d82fd1e19240355d8726025d1ee3d3276789b0a374baca46381dda8ff0b4`.
- Standard DTB for comparison: SHA-256
  `89b1dd57e9f7fd5c6ff71deb86d7ca3d67a7e9394ff08ddccaec164df1593ebe`.
- Restored console opt-in DTB: SHA-256
  `1ff66ae21c994ed02d022f13967113ea081fa73d86fd9ce0f2cc5e99adce95c3`.
- Unchanged loader `435791f`: SHA-256
  `f5712184208467d56cc6626a9d3a17118057e126b34e63113a4f50ff8b0204ab`.

Raw records: `gates/2026-10-21-m3-native-fixed-boot/comparison.json`, four numbered trial directories,
`native-kill/`, `post-native-kill/`, and `gates/2026-10-21-m3-native-console-restore/`.
Uploaded probe hashes and booted kernel identities were verified; deployed boot inputs were
readback-hashed, with outgoing inputs preserved before writes. Restoring the console changed
only the DTB opt-in, not the kernel image. Each launch used one destroy, 30-second settle and one
launch, with no power cycle needed in this comparison sequence.

Reproduce on a fresh **no-fbdev** boot:

```
timeout 50 /tmp/drm-flip-probe --seconds 30
timeout 50 /tmp/drm-flip-probe --seconds 30 --native-abgr
```

Save each output and evaluate with `tools/check-drm-sustained.py`; record remote process exit,
probe hash, kernel identity and independent post-state separately. Do not run these against
fbcon's active CRTC; the probe intentionally refuses to steal it.

## Remaining landing work

Keep production's pin unchanged and kernel PR #13 draft until the kernel series is reviewed and
its base reconciled. Preserve the XRGB limitation in public claims. Physical HDMI confirmation
and a representative non-gray/static colour test are separate remaining checks; the tested native
sustained-rate bottleneck is now resolved without changing clocks.
