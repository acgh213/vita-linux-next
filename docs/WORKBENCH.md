# Vita Linux Workbench

A removable, verified development environment for Vita Linux. The base image stays
a reliable rescue system; the workbench is an expansion layer you activate
explicitly and can remove without drama.

## The contract

```text
Linux boots without USB          -> USB is optional, never a boot dependency
vita-toolkit-session start --rw-workspace
                                 -> transport mounted rw, explicitly
/opt/vita-toolkit                -> hash-verified SquashFS, always read-only
/mnt/vita-storage/vita-workbench -> persistent source/build/log workspace
vita-status                      -> one machine-readable summary
vita-example build hello-native  -> native ARM program builds and runs
vita-dashboard                   -> bounded framebuffer UI
vita-toolkit-session stop        -> hash re-checked, mounts removed in reverse
```

## Architecture: what is mutable and what is not

| Layer | Mode | Rationale |
|---|---|---|
| Initramfs / eMMC / VitaOS | untouched | boot-critical rescue path |
| SquashFS payload at `/opt/vita-toolkit` | **read-only**, hash-verified | trusted toolchain must not drift |
| USB transport filesystem | `rw` only with `--rw-workspace` | explicit opt-in, never implicit |
| `/mnt/vita-storage/vita-workbench/` | read-write, persistent | survives reboot |

A read-write transport does **not** make the payload writable. The SquashFS mount
receives `loop,ro,nosuid,nodev` in every code path; a regression test asserts this.

### Filesystem reality of the workspace

The workspace is only as capable as the filesystems the pinned kernel actually
builds. Verified on `vita-linux-next` at `0d1ba53a4376`:

| Option | State | Consequence |
|---|---|---|
| `CONFIG_SQUASHFS` / `_ZSTD` | `y` | payload mounts read-only through loop |
| `CONFIG_BLK_DEV_LOOP` | `y` | loop mounts available |
| `CONFIG_USB_STORAGE` | `y` | EHCI mass storage works |
| `CONFIG_VFAT_FS` | `y` | small FAT volumes usable |
| `CONFIG_EXFAT_FS` | `y` | **this is what the workbench actually uses** |
| `CONFIG_TMPFS` | `y` | volatile `/tmp`, `/run` |
| `CONFIG_EXT4_FS` | `n` | **no POSIX-permission filesystem available** |
| `CONFIG_OVERLAY_FS` | `n` | no writable overlay over the read-only root |

The practical limit follows from exFAT, not from the workbench tooling:

- no POSIX ownership or permission bits — every file presents as one uid/gid;
- no symlinks, hardlinks, or special files;
- case-insensitive name matching;
- `chmod`/`chown` do not behave as they do on the build host.

This is adequate for the verified uses — source trees, native TinyCC builds,
scripts, logs, captured evidence — and it is *not* a general-purpose Linux root
filesystem. Do not stage a rootfs, a package database, or anything that depends
on Unix metadata onto the workspace and expect it to behave.

Enabling `CONFIG_EXT4_FS` (and deciding whether `CONFIG_OVERLAY_FS` is wanted) is
a separate, bounded kernel lane. It is tracked as a candidate, not a promise, and
the project's rule applies: a config change is not a hardware pass until a
formatted EXT4 volume has been mounted and exercised on the device.

## Explicit versus discovered activation

```sh
# Explicit — skips discovery entirely
vita-toolkit-session start --file /mnt/vita-storage/<payload>.squashfs \
    --rw-workspace --machine

# Discovered — conservative, fails closed
vita-toolkit-session start --rw-workspace --machine

# Inspection only — transport stays read-only, no workspace created
vita-toolkit-session start --machine

vita-toolkit-session status --machine
vita-toolkit-session stop --machine
```

Discovery requires **exactly one** unambiguous removable candidate carrying
**exactly one** payload. Zero candidates, two candidates, two payloads, or an
unsupported filesystem all fail *without invoking `mount`*.

## Payload and mount safety

Verification happens **before** the mount:

1. Resolve the payload without normalising away the given path.
2. Locate checksum metadata: a `<payload>.sha256` sidecar, or a `SHA256SUMS` file
   naming the payload's basename.
3. **Absent checksum metadata is fatal** — the session reports `status=unverified`
   and stops rather than silently trusting the image.
4. A hash mismatch reports `status=checksum_mismatch` and never mounts.
5. After mounting, `VERSION` or `NATIVE-TOOLCHAIN-MANIFEST` must be present, or the
   image is unmounted again and rejected.

At teardown the hash is re-checked. If the payload changed while mounted, `stop`
reports `status=payload_changed` with `payload_unchanged=0`, preserves the evidence
path, and **refuses to claim a clean session**.

## Workspace

```text
/mnt/vita-storage/vita-workbench/
├── src/  build/  log/  capture/  projects/
└── WORKSPACE-INFO   (payload hash, kernel, creation time)
```

`vita-workspace clean` removes a volatile workspace freely, but refuses a
persistent one without `--persistent`, and refuses any path not named
`vita-workbench`. It never touches the payload or unrelated files on the volume.

## Native compile / test

```sh
vita-example list --machine
vita-example build hello-native --machine
vita-dev test --machine --manifest /opt/vita-toolkit/examples/hello-native/vita.project
```

`vita-example build` copies the example **out** of the read-only payload into the
workspace and builds there; the payload is never modified. A stale copy is
replaced, so a rebuild always reflects the payload.

Manifests support `link=-lNAME` (validated — only `-lNAME` is accepted, so a
manifest cannot inject arbitrary compiler arguments).

## Known TinyCC boundaries

The on-device TinyCC is the **bootstrap** compiler. Host
`arm-linux-gnueabihf-gcc` remains the compiler for production artifacts.

| Limitation | Detail |
|---|---|
| static linking | unsupported (`unsupported-modern-glibc-arm-relocations`) |
| `strcspn()` | link error: `can't relocate value at <addr>,1` |
| `memset()` at some call sites | links, then segfaults at runtime |
| implicit `libm` / `libpthread` | not pulled in; declare `link=-lm` / `link=-lpthread` |

Prefer explicit loops over `<string.h>` helpers in code meant to build on-device.
Evidence and the bisection method: `toolkit/examples/fb-safe/README.md`.

## Dashboard

Bounded framebuffer UI; contract in `docs/plans/vita-dashboard-contract.md`.

```sh
vita-dashboard --machine --input-phys vita_syscon_buttons --duration-ms 20000
```

- **CROSS** / D-pad down / `space` / `Enter` / `Tab` — next screen
- **CIRCLE** / `Esc` / `q` — exit

Always stops on its own (default 60 s, max 600 s). Signals restore framebuffer
ownership before exit. Refuses with a clear reason and a distinct exit code rather
than guessing: missing binary `2`, no/unsupported framebuffer `3`, no
identity-matching input device `4`.

**This is plain framebuffer scanout, not SGX or GPU acceleration.** The SGX
aperture remains unaddressable from the non-secure world; the DRM/IFTU page-flip
lane is separate, already-proven work that this application deliberately does not
use yet.

## Bounded execution on this image

The running PSTV image has **no BusyBox `timeout` or `unshare` applet**. Never
write a target-side `timeout` into a workbench procedure. Bound work from the host
(`timeout N ssh …`) and rely on the in-process runtime bounds the programs enforce
themselves. A regression test asserts no payload command depends on these applets.

## Rollback

The workbench is userland-only — no kernel change is involved, so rollback is:

```sh
vita-toolkit-session stop --machine
```

The known-good payload stays on the USB volume under its own filename and is never
overwritten by a candidate. The base Linux image is untouched throughout, and the
persistent workbench is deliberately **not** deleted by ordinary teardown.

## Validation status

See `lab/workbench-baseline-2026-09-07.md`,
`lab/workbench-native-example-2026-09-07.md`, and
`lab/workbench-dashboard-2026-09-07.md` for dated evidence and artifact hashes.
