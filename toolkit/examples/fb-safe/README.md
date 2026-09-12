# fb-safe

The framebuffer render path, taught without any framebuffer write.

This example **never opens `/dev/fb0`**. It reads geometry from sysfs, allocates a
RAM shadow buffer, renders a six-band test pattern into that buffer with bounded
clipping, and reports the byte count it would have written.

## Why a shadow buffer

Measured on PSTV hardware: framebuffer **reads run ~25.7 MB/s** while writes run
~167 MB/s. Rendering that reads back from the framebuffer is roughly six times
slower than rendering into RAM and writing one complete frame.

The rules this example establishes:

1. render into RAM, never into the mapped framebuffer;
2. never read the framebuffer as part of rendering — use the capture tool only for
   before/after evidence;
3. clip every rectangle against the real extent before writing a byte;
4. write one *complete* frame or none at all.

The band loop deliberately requests a width of `width + 64` so the clipping path is
exercised on every run rather than only in a unit test.

## Build and run

```sh
vita-example build fb-safe --machine
vita-dev test --machine --manifest /opt/vita-toolkit/examples/fb-safe/vita.project
```

## Expected output

```text
schema=1
example=fb-safe
framebuffer=1280x720x32
stride=5120
shadow_bytes=3686400
framebuffer_writes=0
status=ok
```

`shadow_bytes` must equal `stride * height` — 3,686,400 on PSTV. That is the exact
byte count a real frame write owes the display.

## TinyCC boundary discovered here (2026-09-07, on hardware)

Two glibc string routines are unusable from the on-device TinyCC on this target,
while the identical source is clean under host `arm-linux-gnueabihf-gcc` with
`-Wall -Wextra -Werror`:

| Call | Failure |
|---|---|
| `strcspn()` | link error: `tcc: error: can't relocate value at <addr>,1` |
| `memset()` in this translation unit | builds, then segfaults at runtime (`run_status=139`) |

The `memset` crash was isolated by stderr bisection to the exact call site, and
ruled out as a link-flag artifact: it reproduces identically with no libraries,
with `-lm`, with `-lpthread`, and with both. Note that `hello-native` *does* call
`memset` successfully, so this is call-site/translation-unit specific rather than a
blanket "memset is broken" claim — the honest statement is that glibc's
ifunc-dispatched string routines cannot be relied on from tcc here.

Both are avoided in this example: the newline trim is an explicit loop and the
geometry struct is zeroed field-wise. Prefer explicit loops over `<string.h>`
helpers in code intended to build with the on-device compiler.

## Next step

`toolkit/examples/vita-dashboard` takes ownership of `/dev/fb0` properly: it saves
the fbcon bind state, unbinds only when it was bound, writes complete frames, and
restores ownership on every exit path.
