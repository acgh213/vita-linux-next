# vita-dashboard

The first interactive framebuffer application on Vita Linux. A small, bounded,
native ARM program that renders status screens and responds to physical input.

Contract: [`docs/plans/vita-dashboard-contract.md`](../../../docs/plans/vita-dashboard-contract.md).

## What it is not

- Not a replacement for the demo cart.
- **Not a claim of SGX or GPU acceleration.** This is plain framebuffer scanout via
  `/dev/fb0`. The SGX aperture remains unaddressable from the non-secure world; the
  DRM/IFTU lane is separate work.
- Not a daemon, not a service, not a network listener.

## Screens

1. **Overview** — kernel, arch, CPUs online, free memory, uptime.
2. **Network / storage** — interface, address, removable storage, toolkit mount.
3. **Build** — compiler identity, workspace path, framebuffer geometry, frame count.
4. **Input** — selected device, identity, name, bounded event count.

## Controls

- **CROSS** / D-pad **down** / `space` / `Enter` / `Tab` — next screen
- **CIRCLE** / `Esc` / `q` — exit

## Runtime bounds

The program always stops on its own:

```sh
vita-dashboard --machine                      # default 60 000 ms
vita-dashboard --machine --duration-ms 15000  # explicit, max 600 000 ms
vita-dashboard --machine --frames-max 100     # stop after N frames
vita-dashboard --machine --forever            # supervised use only
```

`SIGINT`, `SIGTERM`, and `SIGHUP` restore framebuffer ownership before exiting.
A signal arriving during `poll()` is a normal interruption (`EINTR`), not an error.

## Framebuffer ownership

The `fb_owner` state machine enforces the rules that were verified on hardware:

1. probe geometry and the fbcon bind state before touching anything;
2. unbind fbcon **only if it was bound**, and remember that;
3. render into RAM — never read the framebuffer while rendering;
4. write exactly `stride * height` bytes (3,686,400 on PSTV) — a complete frame
   or nothing;
5. restore the original bind state on every exit path.

A short or unreadable framebuffer is refused **before** fbcon is touched, so a
failed probe can never leave the console displaced.

## Refusal contract

The launcher exits nonzero with a clear reason rather than guessing:

| Condition | Exit |
|---|---|
| dashboard binary missing | 2 |
| `/dev/fb0` absent or geometry unsupported | 3 |
| no identity-matching input device | 4 |

Input is selected by the sysfs `phys` identity (default `vita_syscon_buttons`),
never by a hardcoded `event0` — event numbering is not stable across boots or USB
attachment.

## Building

The shipped binary is built by host `arm-linux-gnueabihf-gcc` with
`-Wall -Wextra -Werror`; provenance is recorded in
`share/vita-dashboard/BUILD-INFO`. The sources are kept in the payload so the
program can be rebuilt on-device with the bootstrap TinyCC:

```sh
vita-example build vita-dashboard --machine
```

Note the TinyCC boundary documented in
[`../fb-safe/README.md`](../fb-safe/README.md): glibc's ifunc-dispatched string
routines (`strcspn`, and `memset` at some call sites) are not reliably usable from
the on-device compiler, so this code uses explicit loops throughout.

## Host tests

```sh
sh toolkit/tests/test-vita-dashboard-fb.sh      # ownership state machine
sh toolkit/tests/test-vita-dashboard-render.sh  # clipping + colour order (ASAN)
sh toolkit/tests/test-vita-dashboard.sh         # full application loop
```

The renderer fixtures run under ASAN/UBSAN when available; every out-of-bounds
rectangle, off-screen glyph, and degenerate geometry case is exercised.
