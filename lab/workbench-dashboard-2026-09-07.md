# Workbench session + dashboard status — PSTV 192.168.18.43 (2026-09-07)

**Scope: PARTIAL. The session, workspace, and native-example gates PASSED on real
hardware. The dashboard hardware gate has NOT passed and is not claimed.**

No production promotion. The known-good payload
`6fb29caf00b1bf614399b471e35d719dcad4bdf3a00f6a3c0eb66022aed997a2` was never
overwritten; the candidate was staged under its own filename.

## Candidate identity

| Artifact | Value |
|---|---|
| Candidate SquashFS | `vita-toolkit-workbench-2026-09-07.squashfs` |
| SHA-256 | `1c425b7474f12e41a6c9ac2f7872896123049a08aa70c01c01cddf43233a5bf7` |
| Size | 10,862,592 B |
| Reproducibility | **two builds byte-identical** (`cmp` clean) |
| Toolkit VERSION | 1.3.0 |
| Dashboard ARM ELF (staged) | `afbaef0acf2c705552aa0ed42a42b615308b16f28389eee78b152456d2e95561` |

Upload was hash-verified by read-back on the target before use.

## PASSED — session activation (Task 5.2)

```
status=mounted
source_type=explicit
payload_sha256=1c425b74…        (matches host exactly)
target_options=loop,ro,nosuid,nodev
transport_mode=rw
workspace=/mnt/vita-storage/vita-workbench
```

Measured mount table:

```
/dev/sda1  /mnt/vita-storage  exfat     rw,relatime,…
/dev/loop0 /opt/vita-toolkit  squashfs  ro,nosuid,nodev,relatime
```

- Workspace tree created with all five subdirectories + `WORKSPACE-INFO`.
- Write landed under `vita-workbench/log/` only.
- `touch /opt/vita-toolkit/SHOULD-FAIL` → **`Read-only file system`** — payload
  immutability demonstrated, not assumed.
- `vita-workspace path` → `mode=persistent`, `free_kib=118001616`.

## PASSED — native examples from the mounted payload (Task 5.3)

`vita-example list` reported all three examples. `hello-native` built from two
translation units and ran:

```
accumulator=1024.0000  kernel=6.12.0-g1ee3edf3c90a  arch=armv7l  cpus=4  status=ok
```

`fb-safe`: `framebuffer=1280x720x32  stride=5120  shadow_bytes=3686400
framebuffer_writes=0` — exactly `stride × height`.

Both built into the **persistent USB workspace**, not `/tmp`.

## PASSED — dashboard refusal contract

```
--input-phys no_such_device        -> rc=4  "no input device with phys=…"
missing framebuffer                -> rc=3  "no framebuffer device at …"
--duration-ms 0                    -> rc=2  "duration must be 1..600000 ms"
```

Input inventory: `event0 = vita_syscon_buttons` (the only input device present).

## NOT PASSED — dashboard bounded run (Task 5.4)

First on-hardware run **refused to start**:

```
vita-dashboard: framebuffer is shorter than one frame
rc=4   fbcon_before=1   fbcon_after=1   cpu_online=0-3
```

### Root cause — a real bug in our probe, found by hardware

`fseek`/`ftell` is **not a valid size probe for `/dev/fb0`**. Measured on target:

```
/dev/fb0                     -> crw-rw----  (character device, 29,0)
ftell after fseek(0,SEEK_END) -> 0
dd if=/dev/fb0 bs=4096        -> 900 records = 3,686,400 bytes read fine
```

A character device legitimately reports no size via `ftell`, so the probe computed
"shorter than one frame" for a perfectly good framebuffer.

**The fail-closed design worked exactly as intended:** the refusal happened
*before* fbcon was touched. `fbcon_after=1`, `cpu_online=0-3`, no fault. A wrong
probe could not leave the console displaced.

### Fix applied (host-verified, NOT yet re-gated on hardware)

- Size probe now uses `fstat()` for regular files and `FBIOGET_FSCREENINFO` for
  character devices; `ftell` is gone.
- Frame write moved to POSIX `write(2)` with an explicit short-write loop and
  `EINTR` retry — a partial frame must never reach the screen.
- **Regression test added** (`test-vita-dashboard-fb.sh`, char-device case) so an
  `ftell`-based probe cannot silently return.

Post-fix: host GCC and ARM cross both clean under `-Wall -Wextra -Werror`; all
framebuffer-ownership and application-loop fixtures still pass.

## Outstanding before Task 5.4 can be claimed

1. Rebuild the payload with the fixed `fb_owner.c` (current staged candidate still
   carries the bug) and re-upload with read-back verification.
2. Re-run the bounded dashboard gate.
3. **Requires the operator:** one supervised physical button press to advance a
   screen and one to exit. This cannot be driven over SSH.
4. Task 5.7 reboot-persistence gate.

## Bluetooth pairing caveat for the reboot gate

The rootfs is a **RAM initramfs**, so anything written to `/var/lib/bluetooth`
during a session is lost on reboot. Expect to **redo BT pairing on the PSTV** after
the Task 5.7 reboot, and budget for it before any gate that depends on a Bluetooth
input device (e.g. the Keychron variant of the dashboard gate).

Not verified this session — BT state was not exercised, and the dashboard gate used
the built-in syscon buttons, which need no pairing. Recorded as an expected cost of
the reboot, not as a measured regression. Note also that Wi-Fi/BT share the SD8787
firmware path, so BT work should not be interleaved with a Wi-Fi-dependent gate.

## Target left state

Session active: transport `rw`, payload mounted `ro`, workspace populated. Rollback
is `vita-toolkit-session stop`. Base image untouched — this milestone is
userland-only and involves no kernel change.
