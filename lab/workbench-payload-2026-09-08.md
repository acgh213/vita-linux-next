# Workbench payload build — 2026-09-08

Date: 2026-09-08
Scope: host-side build record for the `2026-09-08` workbench SquashFS payload.
Status: **built, reproducible, staged and mounted on the PSTV. Not yet hardware-gated.**

This record was written retroactively on 2026-09-08 to close a gap: the payload
was built at `2026-09-08 00:18` local and staged to the target, but no lab
record was produced at build time. Everything below was re-verified from the
artifacts and the live target, not reconstructed from memory.

## Source boundary

- Repo: `/home/cassie/projects/vita-linux-next`
- Tip: `89f2e7ba471170a6edca9df8563faa62214dd350`
  (`fix: probe framebuffer size via fstat/FBIOGET, document the workbench`)
- Tip date: `2026-09-07 18:07:36 -0400`
- Working tree: clean (`git status --porcelain` empty)
- Branch `main`, 4 commits ahead of `origin/main` (unpushed at time of writing)
- Builder: `tools/build-vita-toolkit-squashfs.sh`
  SHA-256 `73adfe1024684cdf03a678b39536a223599b647f1afdb9a49ed1fa35e6ff7c1d`

## Artifact identity

| Artifact | Size (B) | SHA-256 |
|---|---|---|
| `dist/vita-toolkit-workbench-2026-09-08-a.squashfs` | 10,866,688 | `63f956ac4c7bd51fd9c7d03fca2d1d3ba8d06897b48847945210c17d54375b72` |
| `dist/vita-toolkit-workbench-2026-09-08-b.squashfs` | 10,866,688 | `63f956ac4c7bd51fd9c7d03fca2d1d3ba8d06897b48847945210c17d54375b72` |

Toolkit `VERSION`: `1.3.0` (unchanged from the 2026-09-07 payload).

### Reproducibility gate

Two independent builds, byte-compared:

```text
cmp dist/vita-toolkit-workbench-2026-09-08-a.squashfs \
    dist/vita-toolkit-workbench-2026-09-08-b.squashfs
-> identical
reproducible=1
```

This is the standing rebuild-twice-and-`cmp` gate for this payload; it passes.

## Delta vs the 2026-09-07 payload

Previous payload: `vita-toolkit-workbench-2026-09-07-a.squashfs`,
SHA-256 `1c425b7474f12e41a6c9ac2f7872896123049a08aa70c01c01cddf43233a5bf7`.

Unsquashed both and diffed. Exactly five paths differ, all attributable to the
single `89f2e7b` fix:

```text
examples/vita-dashboard/src/fb_owner.c   (the fix itself)
libexec/vita-dashboard.arm               (rebuilt binary)
share/vita-dashboard/BUILD-INFO          (binary_sha256 line)
MANIFEST                                 (hashes of the above)
share/README.md                          (workbench documentation)
```

No unexplained content drift. `BUILD-INFO` delta is exactly the one line:

```text
- binary_sha256=afbaef0acf2c705552aa0ed42a42b615308b16f28389eee78b152456d2e95561
+ binary_sha256=7be0e1a06523edbfae925e214fe64e25ec6d81900e00382eab908c8178705e4c
```

### The fix carried by this payload

`fb_owner.c` replaced an `fseek`/`ftell` framebuffer size probe with
`fstat()` (regular files / host fixtures) plus `FBIOGET_FSCREENINFO`
(character devices / the real node). On the PSTV, `/dev/fb0` is a character
device and `ftell()` returns 0 even though a full 3,686,400-byte frame reads
fine — the old probe made the dashboard refuse to start with a bogus
"framebuffer is shorter than one frame".

Note the failure was safe by construction: the size check runs *before* the
fbcon unbind, so the wrong probe could never displace the console.

### Cross-compile check (pitfall 31)

```text
libexec/vita-dashboard.arm: ELF 32-bit LSB pie executable, ARM, EABI5 version 1
(SYSV), dynamically linked, interpreter /lib/ld-linux-armhf.so.3, not stripped
```

ARM, as required. Not a host-toolchain artifact wearing an `.arm` name.

MANIFEST entry agrees with the on-disk binary:

```text
libexec/vita-dashboard.arm 755 72012 7be0e1a06523edbfae925e214fe64e25ec6d81900e00382eab908c8178705e4c
```

## Staging and live target state (verified 2026-09-08)

Payload staged to the USB transport and currently mounted:

```text
/mnt/vita-storage/vita-toolkit-workbench-2026-09-08.squashfs         10,866,688 B
/mnt/vita-storage/vita-toolkit-workbench-2026-09-08.squashfs.sha256
  -> 63f956ac4c7bd51fd9c7d03fca2d1d3ba8d06897b48847945210c17d54375b72
/dev/loop0: /mnt/vita-storage/vita-toolkit-workbench-2026-09-08.squashfs
  mounted at /opt/vita-toolkit  (ro,nosuid,nodev)
```

Host hash, transport sidecar hash, and the live loop-mounted backing file all
agree on `63f956ac…` — three independent points.

Workspace metadata on the persistent transport:

```text
/mnt/vita-storage/vita-workbench/WORKSPACE-INFO
schema=1
payload_sha256=63f956ac4c7bd51fd9c7d03fca2d1d3ba8d06897b48847945210c17d54375b72
kernel=6.12.0-g1ee3edf3c90a
created=2026-09-08T05:28:56Z
```

The recorded `payload_sha256` matches the host artifact.

Target state at verification time:

```text
kernel      6.12.0-g1ee3edf3c90a   (OHCI HCD Phase B candidate)
cpu_online  0-3
mlan0       192.168.18.43/24
hci0        present
transport   /dev/sda1 -> /mnt/vita-storage  exfat rw
payload     /dev/loop0 -> /opt/vita-toolkit squashfs ro,nosuid,nodev
fault_hits  0
```

The RW-transport / RO-payload invariant holds as designed.

`fault_hits=0` after manual verification: the only `dmesg` grep hit is
`printk: debug: skip boot console de-registration.`, the documented
case-insensitive false positive, not a real fault.

## Scope and limits

- This is a **build + staging record**, not a hardware gate result.
- The Phase 5 dashboard gate (Task 5.4) has **not** been run against this
  payload. `dashboard_start`, `fbcon_restored`, and the bounded-runtime
  acceptance criteria remain unmeasured.
- The payload is mounted but the dashboard has not been exercised on hardware
  since the `fstat` fix landed.
- The target is running the **OHCI HCD Phase B candidate**, not the B16
  baseline. That is a deliberate leftover from the Phase B keyboard test; the
  workbench lane does not require it and does not depend on it.

## Timing caveat (see `timer-clock-drift-2026-09-08.md`)

The PSTV monotonic clock runs ~15% fast because `vita.dtsi` declares the A9
timer reference as 144 MHz when the hardware rate is higher. Any wall-clock
interpretation of `--duration-ms` bounds, `timeout` values, or the
`created=` stamp in `WORKSPACE-INFO` on this image is affected. Nothing in
this build record depends on target timing; hashes and sizes are unaffected.

## Next

- Phase 5 hardware gates (5.2 activation, 5.3 native example, 5.4 dashboard,
  5.7 reboot persistence) against this payload.
- Push the four unpushed `main` commits once the gate result exists.
