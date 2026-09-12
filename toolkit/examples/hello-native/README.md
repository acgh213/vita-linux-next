# hello-native

The reference native build for the Vita Linux Workbench. It proves the on-device
compile/link/run loop end to end.

## What it exercises

- two C translation units (`src/main.c`, `src/system.c`) compiled separately and linked
- headers from the target sysroot (`sys/utsname.h`, `unistd.h`, `pthread.h`, `math.h`)
- one pthread worker, created and joined
- one `libm` call per iteration
- explicit `fflush(stdout)` before returning
- a deterministic exit status: `0` on success, distinct nonzero codes per failure

The accumulator is checked, not merely printed: `sqrt(x)^2 / x == 1.0`, so the sum
over `n` iterations must equal `n`. A structurally wrong build fails loudly rather
than emitting plausible numbers.

## Build and run

From the mounted payload:

```sh
vita-example build hello-native --machine
```

Or directly against the manifest:

```sh
vita-dev test --machine --manifest /opt/vita-toolkit/examples/hello-native/vita.project
```

## Expected output

```text
schema=1
example=hello-native
threads=1
iterations=1024
accumulator=1024.0000
kernel=...
arch=armv7l
cpus=4
status=ok
```

## Linking notes

TinyCC does not pull `libm` or `libpthread` implicitly, so the manifest declares
`link=-lm` and `link=-lpthread`. `vita-dev` accepts only `-lNAME` link flags — a
manifest cannot inject arbitrary compiler arguments.

Do **not** add static linking: modern glibc ARM relocations are unsupported by this
TinyCC build (`static_output=unsupported-modern-glibc-arm-relocations` in the
toolchain `BUILD-INFO`).

TinyCC is the bootstrap compiler for on-device work. Host `arm-linux-gnueabihf-gcc`
remains the compiler for production artifacts shipped inside the payload.
