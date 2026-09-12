# Vita Linux Workbench 1.0 Implementation Plan

> **Scope note (2026-09-12):** This dated plan describes the explicit USB
> Workbench milestone. The later SD2Vita game-card path and the expanded rootfs
> toolchain are tracked separately in [`lab/usable-system-2026-09-12.md`](../../lab/usable-system-2026-09-12.md).
> The game-card system is usable on its old-rootfs baseline, while the newer
> full toolchain image is not boot-proven; do not read this plan's earlier
> "no package manager" or "no automatic boot dependency" statements as a
> current status claim.

> **For Hermes:** Use the Vita Linux development workflow with host gates before PSTV action. Execute tasks in order; do not deploy a candidate until its host tests, artifact checks, and rollback procedure pass.

**Goal:** Turn the existing Vita Linux toolkit payload into a coherent, removable development environment with an immutable verified tool layer, a persistent writable USB workspace, native ARM compile/run/test loops, and a small interactive framebuffer application.

**Architecture:** Keep the boot-critical rescue environment, kernel, SSH, networking, and recovery commands embedded in the Buildroot initramfs. Keep the expanding tools and native compiler in the external SquashFS mounted at `/opt/vita-toolkit` read-only. Mount the disposable USB transport filesystem read-write only for an explicit workbench session, using it for source trees, build outputs, logs, captures, and persistent project state. Add one first-party status command and one native framebuffer/input application that compose existing read-only interfaces rather than adding a daemon or package manager.

**Tech Stack:** POSIX shell, BusyBox, Buildroot overlays, SquashFS, SHA-256 manifests, ARM hard-float TinyCC, matched glibc sysroot, Linux evdev, `/dev/fb0`, existing IFTU/page-flip knowledge, host shell/C fixture tests, and the existing hash-verified PSTV deployment workflow.

---

## Product definition

The milestone is complete when a user can perform this sequence on a real PSTV without rebuilding the base image:

```text
Linux boots without USB        -> USB is optional
vita-toolkit-session start --rw-workspace
                                -> transport mounted rw explicitly
/opt/vita-toolkit is mounted   -> verified read-only SquashFS
/mnt/vita-storage/vita-workbench
                                -> persistent source/build/log workspace
vita-status                    -> one useful machine summary
vita-dev project ...           -> native ARM program builds and runs
vita-dashboard                 -> framebuffer UI starts explicitly
buttons/keyboard               -> UI changes view or exits
vita-toolkit-session stop      -> hash checked, then mounts removed cleanly
```

The environment must remain useful if the USB drive is absent. The base image must continue to boot with the embedded diagnostics and the known-good rollback path. The dashboard is a userland application, not a replacement for the existing demo cart and not a claim of SGX acceleration. A normal workbench session may use the USB transport read-write, but the SquashFS image must remain loop-mounted read-only and its hash must be checked again at teardown.

## Non-goals

- No package manager or writable root filesystem distribution model yet. The USB workbench storage is intentionally writable and persistent, but the boot initramfs and toolkit payload are not treated as general mutable system roots.
- No automatic boot dependency on USB storage.
- No LAN control or unauthenticated HTTP mutation service.
- No guessed SGX/Sony secure-world calls.
- No new OHCI kernel work inside this userland milestone; the tested OHCI/HID integration remains its own reviewed lane.
- No audio implementation in this milestone.
- No direct writes to eMMC, VitaOS partitions, the toolkit image, or the USB transport filesystem.

## Existing baseline

Already available and hardware-tested:

- `toolkit/bin/vita-toolkit-mount`
- `toolkit/bin/vita-diag`
- `toolkit/bin/vita-netdiag`
- `toolkit/bin/vita-storage`
- `toolkit/bin/vita-usbinfo`
- `toolkit/bin/vita-inputinfo`
- `toolkit/bin/vita-inputwatch`
- `toolkit/bin/vita-fb`
- `toolkit/bin/vita-fbserve`
- `toolkit/bin/vita-bench`
- `toolkit/bin/vita-control`
- `toolkit/bin/vita-dev`
- `toolkit/share/vita-fbserve.c`
- `toolkit/share/vita-inputwatch.c`
- `tools/build-vita-toolkit-squashfs.sh`
- `tools/build-vita-native-toolchain.sh`
- `toolkit/tests/` host fixture suite

The current native payload has been mounted at `/opt/vita-toolkit` on the PSTV. TinyCC, the matching glibc development sysroot, two-file C linking, pthread/`libm` linking, the diagnostics, and a read-only framebuffer HTTP snapshot have passed on hardware. The payload SHA-256 is recorded in the dated lab validation and must be captured again for each rebuilt candidate.

---

## Acceptance gates

Every phase has two levels of evidence:

1. **Host gate:** shell syntax, fixture tests, manifest checks, architecture checks, reproducible payload checks, and documentation checks.
2. **Target gate:** bounded SSH invocation on the PSTV, exact artifact hash verification, dmesg fault scan, and clean return to either the known-good Linux baseline or VitaOS.

A target gate stops immediately on:

- any checksum mismatch;
- a non-read-only mount;
- ambiguous storage discovery;
- a write to the payload or transport filesystem;
- an unbounded process or network listener;
- a framebuffer ownership violation;
- a kernel fault, Oops, BUG, data abort, soft lockup, or lost SSH;
- a target result that cannot be distinguished from an older image.

Every hardware record must state whether it is a temporary candidate pass or a production promotion. Temporary userland validation never overwrites the known-good rollback binary or image.

---

## Phase 0 — Freeze the baseline and define the handoff

### Task 0.1: Record the known-good artifacts

**Files:**
- Create: `docs/plans/` record only if the plan is missing.
- Inspect: `/home/cassie/projects/vita-linux-r0-clean/dist/vita-toolkit-native-2026-09-02.squashfs`
- Inspect: `/home/cassie/projects/vita-linux-next/lab/toolkit-mounted-validation-2026-09-07.md`
- Inspect: `/home/cassie/projects/vita-linux-r0-clean/Makefile`

Run:

```sh
sha256sum /home/cassie/projects/vita-linux-r0-clean/dist/vita-toolkit-native-2026-09-02.squashfs
ssh -o BatchMode=yes root@192.168.18.43 'uname -a; cat /sys/devices/system/cpu/online; mount'
```

Record the current payload hash, kernel version, CPU range, target IP, mount state, and rollback image identifiers in the implementation handoff. Do not infer that a reboot preserved the same image; verify the runtime every time.

### Task 0.2: Freeze the worktree boundaries

**Files:**
- Inspect: `/home/cassie/projects/vita-linux-next`
- Inspect: `/home/cassie/projects/vita-linux-r0-clean`

Run:

```sh
git -C /home/cassie/projects/vita-linux-next status --short --branch
git -C /home/cassie/projects/vita-linux-r0-clean status --short --branch
```

The public repository receives source, tests, plans, and non-secret lab records. The practical build worktree receives Buildroot builds, generated rootfs artifacts, USB staging, and target deployment. Never use `git add -A` in the outer build worktree, and never copy `board/vita/local/` credentials into the public payload.

### Task 0.3: Add a plan index entry

**Files:**
- Modify: `docs/PROJECT-STATUS.md`

Add an `IN PROGRESS` row or note linking this plan under the Native toolkit / External toolkit payload area. Do not mark the workbench done until the target acceptance sequence has passed.

Run:

```sh
git diff --check
```

Commit:

```sh
git add docs/plans/2026-09-07-vita-linux-workbench.md docs/PROJECT-STATUS.md
git commit -m 'docs: plan Vita Linux Workbench milestone'
```

---

## Phase 1 — Make payload activation a coherent session

The existing `vita-toolkit-mount` is the low-level explicit mount primitive. Preserve it. Add a higher-level session wrapper that discovers the removable transport conservatively, verifies the image, activates the environment, and cleans up in reverse order. Do not make this wrapper run automatically at boot yet.

### Task 1.1: Define the session contract

**Files:**
- Create: `toolkit/bin/vita-toolkit-session`
- Create: `toolkit/tests/test-vita-toolkit-session.sh`
- Modify: `toolkit/README.md`

Supported commands:

```sh
vita-toolkit-session start --machine
vita-toolkit-session start --rw-workspace --machine
vita-toolkit-session status --machine
vita-toolkit-session stop --machine
vita-toolkit-session start --file /mnt/vita-storage/vita-toolkit-native-2026-09-02.squashfs --rw-workspace --machine
```

The wrapper must support an explicit file path and a label-based discovery mode. It must not guess `/dev/sda`, `/dev/sdb`, or an event number. It should use `vita-storage --machine` or a narrow sysfs scan to identify removable partitions, require exactly one unambiguous candidate with the expected label/filesystem, and fail closed otherwise.

`--rw-workspace` is an explicit opt-in for the transport filesystem. In that mode the exFAT/FAT transport is mounted `rw` at `/mnt/vita-storage`, while the SquashFS image is still mounted `loop,ro,nosuid,nodev` at `/opt/vita-toolkit`. The command creates a persistent `/mnt/vita-storage/vita-workbench/` tree for source, build, capture, and log data. Without the flag, transport storage remains read-only for inspection-only sessions.

### Task 1.2: Write failing discovery fixtures

**Files:**
- Modify: `toolkit/tests/test-vita-toolkit-session.sh`

Create fake roots for:

- one removable exFAT partition with the expected payload;
- one explicit `--rw-workspace` session;
- no removable partitions;
- two matching partitions;
- a non-removable disk with a misleading label;
- a missing payload;
- a checksum mismatch;
- an already-mounted target;
- a target with the wrong filesystem;
- a transport mounted read-write while the SquashFS mount remains read-only;
- teardown detecting that the payload file changed while mounted.

Run:

```sh
sh toolkit/tests/test-vita-toolkit-session.sh
```

Expected initially: failure for the not-yet-implemented session command. The fixture must verify that ambiguous discovery does not invoke `mount`, that `--rw-workspace` changes only the transport mount options, and that the SquashFS mount always receives `loop,ro,nosuid,nodev`.

### Task 1.3: Implement verification before mount

**Files:**
- Modify: `toolkit/bin/vita-toolkit-session`
- Modify: `toolkit/bin/vita-toolkit-mount` only if a reusable validation seam is needed.

Required behavior:

1. Resolve the source without normalizing away the user-provided path.
2. Locate a sidecar checksum or manifest using an explicit contract.
3. Verify the SquashFS hash before mounting it.
4. If `--rw-workspace` is present, mount only the transport filesystem read-write; otherwise use `ro`.
5. Mount the image with `loop,ro,nosuid,nodev` at `/opt/vita-toolkit` in either mode.
6. Verify `/opt/vita-toolkit/NATIVE-TOOLCHAIN-MANIFEST` or `VERSION` exists.
7. Create only the documented workbench directories on the writable transport:
   `vita-workbench/src`, `vita-workbench/build`, `vita-workbench/log`, and
   `vita-workbench/capture`.
8. Write session state under `/run/vita-toolkit` and retain the payload hash.
9. Report `status=mounted`, `source`, `payload_sha256`, `target`,
   `source_type`, `transport_mode`, and `workspace` in machine mode.

If checksum metadata is absent, the command must report `status=unverified` and stop rather than silently treating the payload as trusted. A read-write transport does not make the SquashFS writable; the payload file must still be opened and mounted read-only.

### Task 1.4: Add clean reverse-order teardown

**Files:**
- Modify: `toolkit/bin/vita-toolkit-session`
- Modify: `toolkit/tests/test-vita-toolkit-session.sh`

`stop` must verify the payload hash before unmounting the SquashFS, then unmount `/opt/vita-toolkit`, flush and unmount the transport filesystem, and report `payload_unchanged=1`. If the payload changed while mounted, it must report the mismatch, preserve the evidence path, and refuse to claim a clean session. It must refuse to unmount a target whose source does not match the session state. It must be idempotent for an already-stopped session and must leave unrelated mounts untouched.

Run:

```sh
sh toolkit/tests/test-vita-toolkit-mount.sh
sh toolkit/tests/test-vita-toolkit-session.sh
```

Expected: all fake-mount cases pass, including no invocation on ambiguous discovery.

### Task 1.5: Add the session command to the payload and image overlay

**Files:**
- Modify: `tools/build-vita-toolkit-squashfs.sh`
- Modify: `buildroot-vita/board/vita/overlay/usr/local/bin/` only if the rescue image needs the wrapper.
- Modify: `toolkit/README.md`

The session wrapper must be available in the embedded rescue environment only if it can operate without the external payload. The low-level mount helper remains the fallback. Do not make `/opt/vita-toolkit` a required init mount.

Run:

```sh
sh -n toolkit/bin/vita-toolkit-session
sh toolkit/tests/test-vita-toolkit-mount.sh
sh toolkit/tests/test-vita-toolkit-session.sh
tools/build-vita-toolkit-squashfs.sh --source toolkit --manifest-only
```

Commit:

```sh
git add toolkit tools/build-vita-toolkit-squashfs.sh buildroot-vita/board/vita/overlay/usr/local/bin toolkit/README.md
git commit -m 'feat: add verified Vita toolkit session activation'
```

### Task 1.6: Hardware session gate

**Files:**
- Create: `/home/cassie/projects/vita-linux-next/lab/workbench-session-YYYY-MM-DD.md`

On the target, first run an inspection-only session if the storage identity is uncertain. Then run the explicit writable-workbench session:

```sh
vita-storage --machine
vita-toolkit-session start --file /mnt/vita-storage/<payload> --rw-workspace --machine
mount | grep -E 'vita-storage|opt/vita-toolkit'
vita-toolkit-session status --machine
printf 'workbench-write-test\n' > /mnt/vita-storage/vita-workbench/log/session-write-test.txt
cat /mnt/vita-storage/vita-workbench/log/session-write-test.txt
```

Verify the transport mount is `rw`, the SquashFS mount is still `ro,nosuid,nodev`,
the write landed only under `vita-workbench/`, and the payload hash recorded by
the session matches the host hash. Remove the test file through the workbench
command before teardown. The session must detect any direct modification of the
payload file itself and must not claim a clean teardown in that case.

```sh
vita-toolkit-session stop --machine
mount | grep -E 'vita-storage|opt/vita-toolkit' || true
```

Verify the target returns to the prior mount state, the transport is intact, and a dmesg fault scan returns zero hits. Keep the known-good image staged for rollback.

---

## Phase 2 — Give the shell a persistent writable workbench and a useful identity command

The payload is immutable, but a development machine should not throw away every
source file and build result at reboot. In `--rw-workspace` mode, the USB
transport is the persistent writable volume. Programs use a dedicated
`vita-workbench/` subtree for source files, compiler temporary files, logs,
captures, and project state. `/run` or `/tmp` remains the fallback when the
operator intentionally starts an inspection-only session.

### Task 2.1: Define workspace paths and limits

**Files:**
- Create: `toolkit/bin/vita-workspace`
- Create: `toolkit/tests/test-vita-workspace.sh`
- Modify: `toolkit/README.md`

Contract:

```sh
vita-workspace init --machine
vita-workspace path --machine
vita-workspace clean --machine
vita-workspace clean --persistent --machine
```

Use `/mnt/vita-storage/vita-workbench` when the session explicitly enabled
`--rw-workspace`, falling back to `/run/vita-workbench` or `/tmp/vita-workbench`
for inspection-only sessions. Never create directories under
`/opt/vita-toolkit`. Refuse unsafe paths and report free space before starting a
build. `clean` must remove only the workbench directory and must require the
exact expected root. `clean --persistent` must be an explicit action and must
print the path it removed; it must never delete the payload or unrelated files on
the USB volume.

The persistent tree is:

```text
/mnt/vita-storage/vita-workbench/
├── src/
├── build/
├── log/
├── capture/
└── projects/
```

The session creates the directories with restrictive modes where the transport
filesystem supports them and writes a small `WORKSPACE-INFO` record containing
the payload hash, kernel version, and creation time. The record is metadata, not
a substitute for the payload checksum.

### Task 2.2: Add a shell profile fragment

**Files:**
- Create: `buildroot-vita/board/vita/overlay/etc/profile.d/vita-toolkit.sh`
- Modify: `toolkit/toolchain/toolchain-env.sh` only if the generated payload needs a shared function.
- Modify: `toolkit/README.md`

The profile fragment should:

- leave boot untouched;
- add `/opt/vita-toolkit/bin` to `PATH` only when that directory exists;
- set `VITA_TOOLKIT_ROOT=/opt/vita-toolkit` only when mounted;
- define no aliases that hide the underlying commands;
- print nothing during non-interactive SSH commands;
- avoid overwriting a user-provided `CC`, `PATH`, or `TMPDIR`.

Host-test the shell fragment with `sh -n` and a fake mounted/unmounted root.

### Task 2.3: Implement `vita-status`

**Files:**
- Create: `toolkit/bin/vita-status`
- Create: `toolkit/tests/test-vita-status.sh`
- Modify: `tools/build-vita-toolkit-squashfs.sh`
- Modify: `toolkit/README.md`

`vita-status` composes existing commands and reports:

```text
schema=1
product=Vita Linux Workbench
kernel=...
arch=...
cpus_online=...
memory_available_kib=...
framebuffer=1280x720x32
network_interface=...
network_address=...
toolkit_mounted=1
compiler=...
workspace=...
```

Human output may be compact and friendly; `--machine` must be stable. The command is read-only, must not ping, open input devices, write the framebuffer, or start services. If a component is unavailable, report `UNKNOWN` rather than failing the whole identity summary.

### Task 2.4: Add host tests for partial environments

**Files:**
- Modify: `toolkit/tests/test-vita-status.sh`

Test a full fake environment, missing network data, missing framebuffer metadata, unmounted toolkit, and unavailable compiler. Assert stable keys, no writes, and exit status `0` for an inventory with missing optional components.

Run:

```sh
sh -n toolkit/bin/vita-status
sh toolkit/tests/test-vita-status.sh
```

Commit:

```sh
git add toolkit buildroot-vita/board/vita/overlay/etc/profile.d tools/build-vita-toolkit-squashfs.sh
git commit -m 'feat: add Vita Linux workbench shell identity'
```

---

## Phase 3 — Make native development feel like a local development loop

`vita-dev` already supports source lists, object linking, expected exit status, and captured output. This phase should make the workflow discoverable and add examples rather than building a more elaborate build system. In a writable workbench session, its default project/build/log roots should live under `/mnt/vita-storage/vita-workbench/`; in inspection-only mode it may use `/run` or `/tmp` and must say which mode is active.

### Task 3.1: Add a curated example project

**Files:**
- Create: `toolkit/examples/hello-native/vita.project`
- Create: `toolkit/examples/hello-native/src/main.c`
- Create: `toolkit/examples/hello-native/src/system.c`
- Create: `toolkit/examples/hello-native/README.md`

The example must exercise:

- two C translation units;
- headers from the target sysroot;
- one pthread worker;
- one `libm` call;
- explicit stdout flushing only where needed;
- a deterministic exit status.

It must build with:

```sh
vita-dev test --machine \
  --manifest /opt/vita-toolkit/examples/hello-native/vita.project
```

Do not use static linking. Document that TinyCC is the bootstrap compiler and host GCC remains the production-artifact compiler.

### Task 3.2: Add a framebuffer-safe example skeleton

**Files:**
- Create: `toolkit/examples/fb-safe/README.md`
- Create: `toolkit/examples/fb-safe/vita.project`
- Create: `toolkit/examples/fb-safe/src/main.c`

The first example should not write the framebuffer. It should query framebuffer metadata, allocate a RAM shadow buffer, render a small test pattern in memory, and report the intended byte count. This teaches the safe render path before the interactive application owns `/dev/fb0`.

### Task 3.3: Add a target-side tutorial command

**Files:**
- Create: `toolkit/bin/vita-example`
- Create: `toolkit/tests/test-vita-example.sh`
- Modify: `toolkit/README.md`

`vita-example list` prints available examples. `vita-example build hello-native` copies only the selected example into the writable workspace and invokes `vita-dev`. It must reject unknown names and never modify the payload.

Run:

```sh
sh toolkit/tests/test-vita-example.sh
vita-example list --machine
vita-example build hello-native --machine
```

Commit:

```sh
git add toolkit/examples toolkit/bin/vita-example toolkit/tests toolkit/README.md
git commit -m 'feat: add native Vita Linux example projects'
```

### Task 3.4: Native development hardware gate

Build the example on the real PSTV from the mounted payload, verify the source and executable hashes where applicable, run it under a timeout, check the exit status and captured output, and remove the workspace. Repeat once with the payload unmounted to confirm the embedded rescue environment remains usable and fails clearly rather than hanging.

Acceptance:

```text
native-example=PASS
exit_status=0
workspace_clean=1
fault_hits=0
```

---

## Phase 4 — Build the first interactive framebuffer application

This is the visible payoff. It should be a small, bounded, native ARM application with clear ownership rules. It must use the simple framebuffer and evdev input, not SGX and not an always-on service.

### Task 4.1: Freeze the application contract

**Files:**
- Create: `toolkit/examples/vita-dashboard/README.md`
- Create: `toolkit/examples/vita-dashboard/vita.project`
- Create: `docs/plans/vita-dashboard-contract.md`

Initial screens:

1. **Overview:** product label, kernel, CPU count, memory, uptime.
2. **Network/storage:** interface state, IP, removable-storage state, toolkit mount state.
3. **Build:** last native example result, compiler identity, workspace path.
4. **Input:** selected input identity and bounded event counts.

Controls:

- Vita buttons or the validated USB keyboard can advance screens;
- one explicit button exits;
- no input event is consumed from an unverified device identity;
- inactivity does not create a background daemon or network listener.

The application must have a fixed maximum runtime by default, an explicit `--forever` only for supervised use, and a signal handler that restores framebuffer ownership before exit.

### Task 4.2: Write the framebuffer ownership fixture first

**Files:**
- Create: `toolkit/examples/vita-dashboard/src/fb_owner.c`
- Create: `toolkit/examples/vita-dashboard/src/fb_owner.h`
- Create: `toolkit/tests/test-vita-dashboard-fb.sh`

Test the state machine against fake sysfs and a fake framebuffer:

```text
bound -> save geometry -> unbind -> render -> restore bound
unbound -> render -> remain unbound
short framebuffer -> refuse before write
signal/cleanup -> restore original ownership
```

The fixture must enforce the existing rule: save/capture exactly `stride * height`, unbind fbcon only when it was originally bound, and never read or write the wrong byte count.

Run:

```sh
sh toolkit/tests/test-vita-dashboard-fb.sh
```

Expected initially: failure until the ownership module exists.

### Task 4.3: Implement a RAM-shadow renderer

**Files:**
- Create: `toolkit/examples/vita-dashboard/src/render.c`
- Create: `toolkit/examples/vita-dashboard/src/render.h`

Render into a RAM buffer in the known PSTV format: 1280×720, 32-bit `a8b8g8r8` memory order, 5120-byte stride. Keep text and layout simple. Use a bounded clipping helper for every rectangle and glyph write. Do not read the framebuffer as part of rendering; use the existing capture tool only for pre/post evidence.

Host tests must cover clipping, color conversion, screen transitions, and no out-of-bounds writes under ASAN/UBSAN where available.

### Task 4.4: Implement identity and data providers

**Files:**
- Create: `toolkit/examples/vita-dashboard/src/status.c`
- Create: `toolkit/examples/vita-dashboard/src/status.h`
- Modify: `toolkit/examples/vita-dashboard/vita.project`

Use `/proc`, `/sys`, and existing command output only for read-only data. Do not shell out through a network-facing path. Bound every read, use `UNKNOWN` for missing values, and keep the dashboard functional when storage or the toolkit is absent.

### Task 4.5: Implement identity-selected input

**Files:**
- Create: `toolkit/examples/vita-dashboard/src/input.c`
- Create: `toolkit/examples/vita-dashboard/src/input.h`

Reuse the identity-first selection logic from `vita-inputwatch`; do not hardcode `event0` or `event1`. Open the selected evdev node read-only and nonblocking. Poll with a timeout. Treat timeout as a normal bounded result. Exit cleanly on the configured exit button or keyboard escape.

Host fixtures must cover:

- buttons on a different event number;
- missing identity;
- malformed event records;
- repeated events;
- no-event timeout;
- explicit close and signal cleanup.

### Task 4.6: Wire the application loop

**Files:**
- Create: `toolkit/examples/vita-dashboard/src/main.c`
- Modify: `toolkit/examples/vita-dashboard/vita.project`
- Create: `toolkit/tests/test-vita-dashboard.sh`

The loop must:

1. parse options and apply a default runtime bound;
2. select input by identity;
3. inspect framebuffer geometry;
4. allocate one RAM shadow buffer;
5. save original fbcon state;
6. unbind fbcon only when required;
7. render the current screen;
8. write one complete frame explicitly;
9. poll input and advance state;
10. restore framebuffer ownership on every exit path.

Do not add page-flip register access in this first application. A later version may use the already-proven IFTU page-flip path after the basic application has a stable lifecycle.

### Task 4.7: Host validation and cross-build

**Files:**
- Modify: `toolkit/tests/test-vita-dashboard.sh`
- Modify: `tools/build-vita-toolkit-squashfs.sh`

Run:

```sh
sh toolkit/tests/test-vita-dashboard-fb.sh
sh toolkit/tests/test-vita-dashboard.sh
arm-linux-gnueabihf-gcc --version
vita-dev test --machine --manifest toolkit/examples/vita-dashboard/vita.project
file dist/vita-dashboard.arm
readelf -h dist/vita-dashboard.arm
```

The final deployed dashboard must be an ARM ELF. Reject any x86-64 artifact even if it has an `.arm` filename. Prefer host GCC for the production binary; retain the source and project manifest in the payload for on-device rebuilding.

### Task 4.8: Add dashboard to the payload

**Files:**
- Modify: `tools/build-vita-toolkit-squashfs.sh`
- Modify: `toolkit/README.md`
- Modify: `toolkit/examples/vita-dashboard/README.md`

The payload should contain:

```text
toolkit/examples/vita-dashboard/
toolkit/bin/vita-dashboard
toolkit/share/vita-dashboard/BUILD-INFO
```

The installed launcher must refuse to run if `/dev/fb0` is absent, if the geometry is unsupported, or if no identity-matching input device can be selected. It must print a clear reason and exit nonzero rather than guessing.

Build twice from the same source and compare the manifest and SquashFS image. Record the final size and SHA-256.

---

## Phase 5 — Hardware validation on the PSTV

### Task 5.1: Prepare a frozen candidate and rollback pair

**Files:**
- Create: `/home/cassie/projects/vita-linux-next/lab/workbench-dashboard-YYYY-MM-DD/RESULT.md`
- Host artifact directory: `/home/cassie/projects/vita-linux-r0-clean/dist/`

Freeze:

- candidate SquashFS SHA-256 and size;
- source manifest SHA-256;
- dashboard ARM ELF SHA-256 and `file` output;
- known-good toolkit/image hashes;
- start/stop wrapper hashes;
- target kernel version and CPU range.

Do not overwrite the known-good payload. Stage the candidate under a unique USB filename or temporary target directory.

### Task 5.2: Run the writable-workbench activation gate

On the target:

```sh
vita-storage --machine
vita-toolkit-session start --file /mnt/vita-storage/<candidate> --rw-workspace --machine
mount | grep -E 'vita-storage|opt/vita-toolkit'
vita-status --machine
vita-workspace init --machine
```

Verify the transport mount is `rw`, the SquashFS mount is `ro,nosuid,nodev`, and
that the session's workspace points to the persistent USB tree. The payload hash
recorded by the session must match the host hash.

### Task 5.3: Run the native example gate

```sh
vita-example build hello-native --machine
timeout 20 vita-dev test --machine \
  --manifest /tmp/vita-workbench/examples/hello-native/vita.project
```

Record output and exit status. Verify workspace cleanup and no writes under `/opt/vita-toolkit`.

### Task 5.4: Run the dashboard gate

Start the dashboard through a unique temporary wrapper with a bounded lifetime:

```sh
timeout 60 /opt/vita-toolkit/bin/vita-dashboard \
  --input-phys vita_syscon_buttons \
  --duration-ms 60000 \
  --machine
```

Acceptance:

```text
dashboard_start=PASS
framebuffer_geometry=1280x720x32
input_identity=vita_syscon_buttons
runtime_bounded=PASS
fbcon_restored=1
cpu_online=0-3
fault_hits=0
```

Use one supervised physical button press to advance a screen and one to exit. If using the Keychron, first preserve the current storage state and follow the OHCI admission rule: never attach the keyboard while a bus-disturbing OHCI experiment is active. The dashboard gate is a userspace display/input test, not a new USB controller gate.

### Task 5.5: Capture visual evidence without making screenshots the control loop

Use the existing framebuffer capture path after the dashboard has stopped. Validate the exact raw size (`stride * height`) and convert a copy on the host for inspection. Do not use screenshot polling to decide whether the process is alive; use SSH, `/proc/<pid>/exe`, machine output, and exit status.

### Task 5.6: Clean up and rollback

```sh
sync
vita-toolkit-session stop --machine
mount | grep -E 'vita-storage|opt/vita-toolkit' || true
```

Verify the target returns to the prior mount state, the session reports
`payload_unchanged=1`, the persistent workbench remains on the USB device, and a
dmesg fault scan returns zero hits. Do not delete the persistent workbench as
part of ordinary teardown. Keep the known-good image staged for rollback.

### Task 5.7: Verify persistence across a reboot

Before claiming that this is a local development environment, write a small
non-secret marker beneath `vita-workbench/projects/`, record its hash, and stop
the session cleanly. Reboot through the existing controlled Linux/VitaOS
handoff, wait for the target to return, and start a fresh session with
`--rw-workspace`. Verify the marker and its hash survive. Confirm that the
workbench is not mounted automatically: absence of the USB must still leave the
base Linux image bootable, while insertion plus an explicit session restores the
workspace.

Acceptance:

```text
persistent_workspace=PASS
payload_immutable=PASS
usb_optional_boot=PASS
auto_mount=0
fault_hits=0
```

Record this as a separate persistence result in the lab record; it is stronger
evidence than merely seeing a successful write before one teardown.

---

## Phase 6 — Polish and promotion

### Task 6.1: Documentation pass

**Files:**
- Modify: `toolkit/README.md`
- Modify: `docs/PROJECT-STATUS.md`
- Modify: `PROGRESS.md`
- Create: `docs/WORKBENCH.md`

`docs/WORKBENCH.md` should contain:

- the product contract;
- explicit versus discovered activation;
- payload and mount safety;
- native compile/test examples;
- dashboard controls;
- known TinyCC boundaries;
- rollback procedure;
- target validation result and artifact hashes;
- a clear distinction between userland dashboard functionality and SGX/DRM status.

### Task 6.2: Add regression checks

**Files:**
- Modify: `toolkit/tests/`
- Modify: `tools/build-vita-toolkit-squashfs.sh`

The host suite must verify:

- shell syntax for every new script;
- no credential or `board/vita/local/` paths in the payload manifest;
- all deployed ELF files are ARM;
- all machine-output commands have stable schema markers;
- the session cannot mount writable;
- dashboard cleanup restores fbcon state;
- two identical builds produce identical manifests and images.

Run:

```sh
for test in toolkit/tests/*.sh; do sh "$test"; done
sh -n tools/build-vita-toolkit-squashfs.sh
git diff --check
```

Use `sh "$test"` rather than relying on executable bits; the suite has previously contained non-executable test scripts.

### Task 6.3: Promote only after the full gate

Promotion requires:

- clean public repository worktree;
- clean practical build worktree except intentional generated artifacts;
- exact source commit and payload manifest;
- two-build reproducibility result;
- target native example pass;
- target dashboard pass;
- zero fault hits;
- clean framebuffer ownership;
- clean toolkit teardown;
- rollback or VitaOS recovery verified;
- dated lab record committed and pushed.

Only then change the matrix from `IN PROGRESS` to `DONE` and update the short progress pointer.

---

## Dependency graph

```text
Phase 0: baseline + plan
    ↓
Phase 1: verified toolkit session
    ↓
Phase 2: workspace + vita-status
    ↓
Phase 3: curated native examples
    ↓
Phase 4: dashboard host implementation
    ↓
Phase 5: PSTV hardware gate
    ↓
Phase 6: docs, regressions, promotion
```

The dashboard may be developed against a temporary host-built payload before Phase 3 is fully polished, but it must not enter the target gate until the session, workspace, and native example contracts are stable.

## Recommended execution order

1. Freeze the current mounted payload and target baseline.
2. Implement and fixture-test `vita-toolkit-session`.
3. Add `vita-workspace` and `vita-status`.
4. Add the two-file native example and `vita-example` selector.
5. Build the framebuffer-safe dashboard ownership module and renderer under host fixtures.
6. Add identity-selected input and the bounded dashboard loop.
7. Build the dashboard with host ARM GCC and verify the ELF architecture.
8. Rebuild the payload twice and compare hashes.
9. Run the PSTV session/native-example gate.
10. Run the supervised dashboard gate with one physical interaction.
11. Teardown, fault-scan, rollback, document, and promote only if every acceptance criterion passes.

## Final shape

At the end of this plan, Vita Linux should not merely have “some tools on a SquashFS.” It should have a recognizable workbench:

- the base image is a reliable rescue/core system;
- the removable payload is a verified expansion layer;
- the shell makes the tools discoverable;
- the compiler can build locally;
- the machine can describe itself;
- the framebuffer can host an actual interactive program;
- the whole thing can be removed and rolled back without drama.
