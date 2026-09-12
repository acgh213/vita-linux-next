# vita-dashboard contract

Frozen interface for the first interactive framebuffer application on Vita Linux.
This document is the acceptance reference; the implementation must not quietly
widen it.

## What it is

A small, bounded, native ARM userland program that renders status screens into a
RAM shadow buffer and writes complete frames to `/dev/fb0`, advancing screens on
physical input.

## What it is not

- Not a replacement for the demo cart.
- Not a claim of SGX or GPU acceleration — this is simple framebuffer scanout.
- Not a daemon, not a service, not a network listener.
- Not a page-flip user: it writes `/dev/fb0` directly. The proven IFTU page-flip
  path (M2 Gate C/D) may be adopted only after this application has a stable
  lifecycle.

## Screens

1. **Overview** — product label, kernel, arch, CPU count, memory, uptime.
2. **Network / storage** — interface, address, removable storage, toolkit mount.
3. **Build** — compiler identity, workspace path, last native example result.
4. **Input** — selected input identity and bounded event counts.

## Controls

- Any of D-pad down / CROSS / keyboard `space` / `Enter` / `Tab` advances a screen.
- CIRCLE / keyboard `Esc` / `q` exits.
- Input is only consumed from a device selected **by identity**, never from a
  hardcoded `event0` / `event1`.
- Inactivity never spawns a background process or listener.

## Runtime bounds — non-negotiable

- Default maximum runtime is **60 000 ms**. The program exits on its own.
- `--duration-ms N` sets an explicit bound, capped at 600 000 ms.
- `--forever` exists only for supervised use and must be typed explicitly.
- `SIGINT`/`SIGTERM`/`SIGHUP` restore framebuffer ownership before exit.

## Framebuffer ownership rules

Derived from the verified PSTV behaviour (pitfall 23) and the fb-safe example:

1. Read geometry from sysfs. Refuse to run if `/dev/fb0` is absent or the geometry
   is unsupported.
2. Read the fbcon bind state **before** touching anything.
3. Unbind fbcon **only if it was bound**; remember that fact.
4. Render into a RAM shadow buffer. Never read the framebuffer while rendering.
5. Write exactly `stride * height` bytes — a complete frame or nothing.
6. On every exit path, including signals, restore the original bind state.

A short or unreadable framebuffer must be refused **before** any write.

## Refusal contract

The launcher exits nonzero with a clear reason, never guessing, when:

- `/dev/fb0` is absent or too small for one frame;
- geometry is missing, malformed, or implausible;
- no identity-matching input device can be selected;
- the requested duration is invalid.

## Machine output

`--machine` emits stable `key=value` lines, including:

```text
schema=1
application=vita-dashboard
framebuffer=WxHxBPP
stride=N
frame_bytes=N
input_identity=...
input_device=...
fbcon_was_bound=0|1
fbcon_restored=0|1
frames_written=N
screens_visited=N
input_events=N
runtime_ms=N
runtime_bounded=PASS|FAIL
status=ok|<reason>
```

## Acceptance (Task 5.4)

```text
dashboard_start=PASS
framebuffer_geometry=1280x720x32
input_identity=vita_syscon_buttons
runtime_bounded=PASS
fbcon_restored=1
cpu_online=0-3
fault_hits=0
```
