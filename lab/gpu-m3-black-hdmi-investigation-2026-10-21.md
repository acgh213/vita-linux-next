# Physical HDMI gate: connected TV remains black

Cassie reported that nothing appeared after the console-marker test, then clarified that the TV
was connected. Treat physical output as FAILED, not merely awaiting confirmation. Linux-side
fbcon registration, shadow readback, selector transitions and events did not prove visible output.
Native repeated-presentation measurements remain valid as controller/ABI measurements, not proof
of visible frames on the TV.

## Evidence and first hypothesis

Read-only diagnostic kernel `cf2caedc3e0e` adds `iftu_hw` (known mapped plane/control registers)
and `iftu_buffers` (the two actual CDRAM buffers, not the fbdev shadow). It adds no new register
writes. Raw evidence: `gates/2026-10-21-m3-black-hdmi-diagnostics/`.

Actual CDRAM config 1 readback contains legible Linux console text. Both configurations contain
rendered pixels. Config 0 address is 0x20000000; config 1 is 0x20384000. Geometry, stride-related
width, source format 0x10, 1x scale and inherited destination placement (26,15) match the expected
configuration. Bus control/control2 both read 1. Alpha value at plane +0x8c and alpha control at
+0xa0 both read zero.

The preserved libbaremetal source (bundle master c16a1a92ff07dff2fa0840bfab2454eaff2cf39e,
`libbaremetal/src/iftu.c:95-106`) uses alpha value 0 and alpha control 1 for opaque alpha=256.
That produced the first specific hypothesis: inherited alpha configuration might explain black
output despite valid CDRAM text. This is a hypothesis, not an established root cause.

## Bounded opaque-alpha test — visual result pending

Kernel `c69dda67b908` adds a default-false `vita_iftu.restore_opaque=1` boot opt-in. It first requires
inherited alpha value=0, then performs ONE documented write: plane A +0xa0 = 1, matching the
loader's alpha=256 path. No timing, PLL, timer, SMC or bridge programming was changed. The DTB
only appends that boot parameter to the already-tested fbdev opt-in DTB.

The system booted successfully, with fbcon and SSH healthy. However **the register read zero
both immediately before and immediately after the write**. This does not verify the desired
hardware state; the register could have nontrivial readback semantics or the write may not have
landed. Therefore the original zero read cannot by itself establish transparent alpha. Do not
present this as a confirmed fix or add more writes before observing the result.

A static marker was written to tty1: `PSTV LINUX - HDMI ALPHA CHECK`, `PYRRHA-OPAQUE`.
The next required observation is Cassie's TV result. If still black, return to comparison against
the genuinely human-confirmed earlier setup, not clock changes or guessed secure calls.

## Inputs

- Diagnostic zImage: 32,191,416 bytes, SHA-256
  f0934dda4087621acf96c76501f5407a4ae84b491241edcb6bf85e3f27e29349.
- Opaque-test zImage: 32,191,304 bytes, SHA-256
  1f37351ce97f16291c946f9452e80d2222375a43c97889e56b52a1c9b0282ddb.
- Opaque-test DTB: 6,432 bytes, SHA-256
  b178404f82a2b554a91158764cea7d7783da6de0e2a8d1b4bb19093fdcb6faa6.
- Unchanged payload: 105,928 bytes, SHA-256
  f5712184208467d56cc6626a9d3a17118057e126b34e63113a4f50ff8b0204ab, loader ref 435791f.

Inputs were preserved locally and on-device before upload and readback-hashed. Each launch used
one destroy, 30-second settle and one launch; no power cycle was needed. W=1 driver builds and
full zImage/DTB builds succeeded. Driver series remains draft; production pin unchanged.

Earlier human-confirmed six-band HDMI and simplefb rollback used protected loader e9934c46...
with kernel 707317ba24f2 / c477a244a49b, not the current loader binary. That is an actual
provenance difference to investigate if this alpha test does not restore visible output.
