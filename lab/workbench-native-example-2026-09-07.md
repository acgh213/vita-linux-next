# Workbench native example gate — PSTV 192.168.18.43 (2026-09-07)

**Scope: temporary candidate PASS on real hardware. No production promotion, no
persistent install.** Tools and examples were staged under `/tmp/vita-wb/` and
removed afterwards; the mounted payload at `/opt/vita-toolkit` was never modified.

## Target identity (measured at gate time)

```
kernel      6.12.0-g1ee3edf3c90a
arch        armv7l
cpus_online 0-3
toolkit     /opt/vita-toolkit  ro,nosuid,nodev  VERSION 1.1.0
transport   /dev/sda1 -> /mnt/vita-storage  exfat ro
```

## Artifact identity (host == target, verified before execution)

| File | SHA-256 |
|---|---|
| `vita-dev` | `648debfe89327787a1d22f0f653f2df6fec39ef480b9ac2744005b5fb21ad037` |
| `hello-native/src/main.c` | `2e673803f6d5a31f20b47cf4d0c0dfef3d67e0070f86672aa0a69fe9d22773b3` |
| `hello-native/src/system.c` | `62d1ca5a3abac1fd6f516b1301f108844c26fa305769681c4eadb8432a3d88c7` |
| `fb-safe/src/main.c` (final) | `0b3b06facc2828760ce5ab5cdea596f3617eb0a26ed37291d296177c8cf91a98` |

## hello-native — PASS

Built on-device by TinyCC from two translation units with `-lm -lpthread`:

```
status=pass  source_count=2  expected_status=0  run_status=0
```

Program output:

```
schema=1
example=hello-native
threads=1
iterations=1024
accumulator=1024.0000
kernel=6.12.0-g1ee3edf3c90a
arch=armv7l
cpus=4
status=ok
```

The accumulator is *checked*, not merely printed — a structurally wrong build fails
rather than emitting plausible numbers.

## fb-safe — PASS (after a real toolchain bug was found and fixed)

```
status=pass  run_status=0
framebuffer=1280x720x32  stride=5120  shadow_bytes=3686400  framebuffer_writes=0
```

`shadow_bytes` equals `stride * height` exactly. No framebuffer write occurred.

### TinyCC boundary discovered (this is a genuine finding, not a workaround)

The first on-device run **failed** (`run_status=139`, segfault) while the identical
source compiled clean under host `arm-linux-gnueabihf-gcc -Wall -Wextra -Werror`.
Two distinct glibc-vs-tcc problems were isolated by bisection on hardware:

1. `strcspn()` — **link** error `tcc: error: can't relocate value at <addr>,1`.
2. `memset()` in that translation unit — links, then **segfaults at the call site**.

The `memset` crash was localized by stderr-marker bisection to the exact statement
(`RG-ENTER` printed, `RG-MEMSET-OK` did not), and explicitly ruled out as a
link-flag artifact: it reproduces with no libraries, with `-lm`, with `-lpthread`,
and with both.

**Honest scope of the claim:** `hello-native` calls `memset` successfully on the
same box, so this is *not* "memset is broken on the Vita". The supportable statement
is that glibc's ifunc-dispatched string routines cannot be relied on from the
on-device tcc, and code intended to build there should use explicit loops.

Both call sites were rewritten (explicit newline-trim loop; field-wise struct
zeroing). Re-verified: host GCC clean under `-Werror`, host ARM cross-build produces
an ARM ELF, on-device tcc `run_status=0`.

## Payload-absent behaviour — PASS

With the toolkit root pointed at a nonexistent path, every command fails *clearly
and immediately* rather than hanging:

```
vita-dev info      -> status=missing_toolchain_env   rc=1
vita-example list  -> status=examples_unavailable    rc=1
vita-status        -> toolkit_mounted=0 compiler=UNKNOWN  rc=0 (inventory still works)
```

## vita-status on real hardware — PASS

```
product=Vita Linux Workbench   kernel=6.12.0-g1ee3edf3c90a   arch=armv7l
cpus_online=0-3                memory_available_kib=377620   framebuffer=1280x720x32
network_interface=mlan0        network_address=192.168.18.43/24
toolkit_mounted=1              toolkit_version=1.1.0
compiler=/opt/vita-toolkit/bin/cc
```

## Cleanup and fault scan

Staging directories removed. `cpu/online` = `0-3`.

dmesg fault scan: the single case-insensitive match is
`printf: debug: skip boot console de-registration` — the documented false positive
(a `debug:` string, not a `BUG:`). Verified by hand; **real `fault_hits=0`.**

## Bounded-execution note

The running image has **no BusyBox `timeout` applet** (see the Phase 0 baseline), so
`timeout` on the target returns `sh: timeout: not found`. Every command in this gate
was bounded from the host with `timeout N ssh …` instead. Do not write target-side
`timeout` into workbench procedures for this image.
