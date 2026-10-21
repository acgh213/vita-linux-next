# Loader observability — what a loader-phase log can and cannot reach

**Status: all four channels below are closed on the PSTV. None is a bug waiting to be
fixed — each was disproven on hardware, and the failure mode of each is recorded here so
nobody spends another evening re-deriving it.**

Everything in this document concerns the **baremetal loader phase** — from
`launch PLGINLDR0` until Linux owns the console. Once Linux boots, display, console and
SSH all work normally (`docs/PROJECT-STATUS.md` records HDMI console login). The gap is
strictly the window before that, which is exactly the window in which a loader experiment
produces its evidence.

## The device facts that make this hard

- The loader runs with **no MMU** — no `mmu_enable`, no TTBR, nothing in the loader or
  `libbaremetal` sets one up. Its stores go to physical memory directly and cannot fault,
  which removes "the write crashed" as an explanation for a missing value.
- The framebuffer is at `0x20000000` (1280 px, stride 5120, 3,686,400 B), **outside** the
  DTB memory node (`0x40200000`, size `0x1FE00000`).
- `/proc/iomem` on the running kernel reports System RAM as `0x41000000-0x5fefffff`.
- The two machines in the family behave differently here: on a **Vita** the OLED is left
  running by VitaOS, so the loader's framebuffer writes become visible. On a **PSTV** the
  output is HDMI and nothing scans out until something configures it.

## Channel 1 — framebuffer, read after boot

**Closed, and it fails silently in a way that looks like success.**

The loader writes the framebuffer at the correct address and the writes land — a capture
taken after boot changes between runs, so the loader demonstrably drew something.
But Linux `fbcon` repaints the same region during boot, so what you read back after
SSH comes up is the **Linux console grid**, not the loader's text.

Diagnostic that identifies this: the Linux console grid starts at `y=3` with a 16 px line
pitch (45 full rows, ink `0xAA`), while the loader draws starting around `y=200` with a
20 px pitch. If your capture matches the former, you captured the wrong thing.

Glyph decoding was attempted and abandoned: the device font is **not** the kernel's
`font_8x16` (0 of 1793 captured cells matched, both bit orders), so
`fb-text-decode.py` cannot read this screen without a matching font table.

## Channel 2 — file, via FatFs

**Structurally impossible in this port.** `FatFs/diskio.c::disk_write()` is a stub
returning `RES_PARERR`, and `ffconf.h` ships `FF_FS_READONLY=1`. There is no SD write path
in the loader at all. Enabling writes and adding a `LOG()` file sink produces a payload
that builds, boots, and writes nothing — confirmed by probing the card over FTP for the
log file afterwards (absent in every candidate location).

Adding a write path is possible but is real work on a filesystem port, not a logging
toggle.

## Channel 3 — display, during the loader phase

**Closed on PSTV.** `display_init()` is already called by the loader (it has been since
`94cefdf`) and adding a second, explicit `display_init_hdmi()` call changed nothing —
the screen stays black. There is nothing on the HDMI side to inherit, unlike the Vita's
OLED, so the loader's text never reaches a display.

**Correction worth keeping:** an earlier write-up blamed a *missing* `display_init()` and
claimed to fix it. That was wrong — the call was already present, and the commit was
reverted. Do not re-open this line without a new mechanism, not a new call site.

## Channel 4 — RAM mailbox, read after boot

**Closed.** The design is sound in principle: the loader publishes
`[magic "VDDG"][len][tag "LOG."][reserved][text]` with the tag written last, cleans L1 via
CP15, and a small `mmap`-based `/dev/mem` reader picks it up after Linux is up.

Four candidate addresses were published in a single run and all read back after boot:

| address | region | after boot |
|---|---|---|
| `0x20400000` | CDRAM past the framebuffer | garbage |
| `0x21000000` | CDRAM past display buffers | `0xFFFFFFFF` |
| `0x22000000` | CDRAM, well clear | `0xFFFFFFFF` |
| `0x40800000` | main DRAM below System RAM | **kernel pointers** |

`0x40800000` containing kernel pointers is the conclusive one: the kernel builds early
page tables in the low DRAM region and overwrites anything the loader left there. The
CDRAM candidates read as unbacked or as memory the display path owns.

Note that `/dev/mem` write/read round-trips *do* work at these addresses from Linux, which
is what makes this channel tempting — the address is writable, it simply does not hold
what the loader put there across the handoff.

`tools/loader-log-read.c` (in the research repo) is the reader used for this test, kept
because it is the right tool if a mailbox is ever revisited with a location the kernel is
guaranteed not to touch.

## What actually reopens loader observability

A **UART**. `SERIAL.md`, `serial-bridge.sh` and `serial_log.py` in this repo already
assume that workflow. Until one exists, the honest position is:

- a loader experiment's *execution* can be confirmed indirectly (did Linux boot? did the
  framebuffer change? did the box stay up?)
- its *output* cannot be read at all
- therefore **no gate decision, branch, or error code from a loader experiment can be
  claimed as observed**

The practical consequence, stated plainly: a payload that boots Linux is evidence that it
ran, not evidence that it did the right thing. Every conclusion drawn from loader-phase
behaviour without a UART is inference, and should be labelled as such.

## Deployment trap

`ux0:/baremetal/payload.bin` is **inert** on this device series; the live path is
`ux0:/linux/payload.bin`. Deploying to the wrong one produces a device that appears to
hang while actually running a stale payload — this is what generated the withdrawn
"`0x88E` write hangs the box" claim. Use `tools/deploy-payload.py`, which targets the live
path and refuses a baremetal target.
