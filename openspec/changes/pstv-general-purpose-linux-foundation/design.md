# Design

## Context

See `proposal.md` for motivation and the bounded first milestone. This design is a **candidate implementation contract**, not a new hardware result. It is required because root selection, private provisioning, nested filesystems and terminal shutdown actions cross trust and lifecycle boundaries.

### Source anchors and chronology

Paths prefixed **N:** resolve under `/home/cassie/projects/vita-linux-next`; **R:** under `/home/cassie/projects/vita-linux-research`. The planning snapshot is N HEAD `91378d570a06aa354482f72b460eb4e45f28b3f7`, R HEAD `8685d20e9434dd1c644d6a03e59c927e61748041`. These are inspected revisions, not permission to reset a future checkout to them.

- **E1 — early architecture, not current device state:** N:`README.md:7–22`, `docs/PROJECT-STATUS.md:3–22`, `docs/HARDWARE-ROADMAP.md:3–22`, `docs/architecture/linux-system.md`, `docs/plans/2026-09-13-linux-system-roadmap.md`, and `docs/evidence/2026-09-13-system-audit.md`. They record the rescue baseline, exFAT preservation, separate distro root and larger A0–H roadmap. Their RAM-only/Debian-unproven assertions predate E4. The old roadmap's proposed `system/` paths do not exist at the inspected HEAD.
- **E2 — revised decisions and host-only work:** R:`lab/HANDOFF-2026-09-13-sysvinit-p0-2.md:26–38,51–58,94–119`. Sysvinit, not a cgroup kernel rebuild; first image 8 GiB, superseding 16 GiB. Its claim that P0-1 is still absent is superseded by E5. Its older loop-detach graph is not the later root-loop design.
- **E3 — SD fault and USB pivot:** R:`lab/hw-trial-2026-09-13/HANDOFF-2026-09-13-usb-pivot.md:13–25,54–103,107–151,224–268`. Linux card timeouts/no enumeration versus VitaOS readability are recorded observations; a specific card/adapter/timing cause remains **inferred**, not isolated. Zero removal events without an enumerated card were a false pass. USB override access from VitaOS was still unknown; a separate host/card reader is the conservative recovery path.
- **E4 — later dated PSTV trial:** R:`lab/usb-debian-2026-09-13/RESULTS.md:131–212`, `MANIFEST.md:6–32`, `device-verify.log:1–10`, `boot-debian.log:1–26`, `shutdown-order-fix.log`, `native-acceptance.sh`, `restart-test.sh`. The report records Debian/bookworm PID 1, USB ext4 loop root, apt install/run, native programs, rescue override and one marker-preserving restart. Raw hash/readback/SSH-cycle logs corroborate parts, not every report assertion. A script is a method, not proof it completed. The report's filesystem file-count prose differs from `device-verify.log` (22878 versus 22872); use the raw log for that measurement, not the prose count. No current-HEAD or fresh hardware pass is inferred.
- **E5 — inspected implementation:** N:`buildroot-vita/board/vita/overlay/init`, `.../usr/share/vita-boot/select-root`, `debian/overlay/etc/init.d/{vita-boot-confirm,vita-loopdetach,vita-root-sync,vita-card-mounts}`, `debian/insserv-overrides/{sendsigs,umountroot,umountfs}`, `debian/README.md`, `debian/packages.txt`; commits `4e5db93`, `dde8747`, `6a0bdb7`, `708f223`, `42cbfaf`, `91378d5`. Existing fixtures: N:`tests/test-init-usb-root.sh`, `tests/test-select-root-durability.py`, `tests/test-debian-lifecycle.py`. These were read, not rerun in this planning session.
- **E6 — build/provenance boundaries:** N:`Makefile:225–233,254–281`, `.github/workflows/build.yml:47–68,85–105`, `tools/vita-manifest.sh`, `tools/lib/vita-extract-initramfs.py`, `buildroot-vita/board/vita/post_build.sh`. Current CI deliberately omits embedded initramfs and builds DTBs; it is not a full image or device gate. The manifest helper already provides ELF-symbol-based initramfs extraction and offline readback comparison; extend/reuse it, do not scan compressed magic or copy a second extractor.
- **E7 — protected hardware and independent research:** R:`README.md:36–38` records fully validated PSTV Bluetooth HID. N:`lab/usb-production-multidevice-2026-09-12.md` and `docs/PROJECT-STATUS.md:45–59` preserve USB input, framebuffer, networking, SMP and timer baselines. R:`lab/PATHS-FORWARD-2026-08-30.md` is the cross-subsystem research map, not a current system acceptance report. Later dated USB/display records supersede its older open gates. No disconnected-controller observation withdraws historical HID success.
- **E8 — timer clock provenance: already corrected, already inside the pin, do not re-apply:** R:`lab/timer-provenance-2026-09-08/{periphclk-divider-audit.md,clockgen-driver-analysis.md}` traced the inherited 144 MHz value to a Cortex-A9 pattern shared with unrelated SoCs and found the ÷2 relationship unprovable statically (no firmware path selects it; the A9 PPB is set by reset defaults alone). R:`lab/linux-ratio-capture-2026-09-09/LADDER-F-RESULT-2026-09-11.md:3,64–96` then measured `PMCCNTR:GT` directly on the running kernel twice (median 1.999856 / 1.999865, 72 ppm residual, analyzer suite 13 passed) and records the provenance gate as closed with N=2. The fix is commit `8f98414c7249` (“ARM: dts: vita: correct the inherited private timer clock rate”, branch `topic/vita-timer-rate`) and is **already present in the pinned gitlink `37b9348710df…`**: `arch/arm/boot/dts/vita.dtsi` defines `periphclk` at 165667500 Hz with both `global_timer@1a000200` and `local_timer@1a000600` referencing `&periphclk`, identical for those lines to the topic commit. Deployed corroboration, not a new result: R:`lab/usb-debian-2026-09-13/baseline-dmesg.txt:32,37` shows `sched_clock: 64 bits at 166MHz` and `BogoMIPS 331.33 (lpj=1656675)` — the corrected clock; the superseded value produced lpj=1440000 / 288.00 BogoMIPS. Boundary, and the only genuinely attackable surface: N=2 is the *measured* part (72 ppm residual); the absolute constant is a boot-configuration model. R:`lab/timer-provenance-2026-09-08/clockgen-ssc-mismatch.md` records that `clockgen.skprx` provably writes the P1P40167 SSC register to code `001` (mean −0.25%) on every VitaOS boot, while the preserved Linux-time read shows the power-up default `010` (mean −0.5%); why that blind, never-read-back write does not stick (latch-at-power-up versus a write-protected register) is RE inference, and the −0.25% counterfactual is excluded by the long baseline at roughly 250σ. R:`lab/timer-provenance-2026-09-08/baseclk-semantics.md:273–314` keeps three items at hypothesis rather than proof: the −0.5% mean assumes a symmetric modulation profile (hard bound: strictly inside −1.0%..0%), selector 7 is taken as exactly 333.000 MHz rather than 333.333 MHz, and the handheld models' own reg2 has never been read — all three DTBs inherit this constant through the shared DTSI, and R:`lab/timer-implementation-2026-09-11/RESULT.md:34–36` requires handheld validation rather than inferring it from a compile or a PSTV measurement. Duration-based acceptance tolerances and timeouts in this change must therefore be sized with roughly ±0.25% worst-case headroom for a different SSC state, not treated as an exact oscillator; closing the latch-versus-write-protect question needs an I²C capture or a controlled reg2 write-and-read-back on hardware, which stays out of scope here. This change must not re-apply, re-derive or bundle a timer/ref-clock DT edit.

Recorded E4 artifact identities (historical only):

- Kernel source gitlink `37b9348710dfe1751dae0ef0fd2954b2714d08d4`; deployed zImage SHA-256 `731c417fa98a617ec1a493964779d0cd1c42a1adb64a1a134329a58c1221e307`.
- PSTV DTB SHA-256 `72ddd25262dcbeb827059eea246fa9ea03cf56020f375665549d147106ee0317`.
- Transferred root image SHA-256 `3e922579dfcb0251745f8e1b50dbda8f9512c9ef04680fd111e5c75a4ac1bc31`; this is a pre-mutation transfer identity, not its expected current hash.
- Pre-USB rollback zImage SHA-256 `0bfcbfbfe9e5150c1157bf0de7dd86aff4803f8ba465e2f360de8b7100477724`.

### Gaps visible in current source

These are **source-observed risks**, not newly reproduced target failures:

1. `/init` permits unpinned media fallback, mounts the container rw before selection, assumes loop7 and detaches it, skips filesystem checking if its tool is missing, and runs `e2fsck -p`. It checks executable presence rather than a complete root identity/ABI contract. Cleanup results are mostly discarded (E5).
2. The selector has durable-append checks and retry logic, but its records do not bind confirmation to a candidate/attempt identity; an absent/empty/unreadable override can collapse to the same value. Legacy record migration and partially durable confirmation need explicit tests (E5).
3. Header assertions do not execute the complete generated SysV graph. A failing stop script alone does not prove `halt` or `reboot` is prevented. `vita-card-mounts` logs failed unmounts then returns success; enumeration pipelines need deliberate failure tests (E5).
4. `debian/README.md` gives a manual recipe, not a pinned builder. `packages.txt` is unversioned; source-only checks do not prove package provenance, service registration, mounted-image integrity or secret-free distribution (E5/E6).
5. The rescue `S06toolkit` independently starts an untracked SD waiter, may mount unapproved storage rw and ignores several stop failures. Foundation rescue must inhibit that automatic lane; rewriting the entire legacy Workbench is not necessary for this slice.

## Goals / Non-Goals

**Goals:** one reviewable PSTV USB-root profile; shared host/CI gates; generated-image validation; a frozen candidate pair; exact-candidate package persistence and recovery evidence. Every acceptance result names its venue: **H** host fixtures, **I** image/build inspection or host QEMU chroot, **P** PSTV hardware, **V** Vita hardware. H/I never discharge P/V.

**Non-Goals:** automatic migration of today's mutable root, byte-identical ext4 images, production release/security-support promises, full backing-store unmount, root hot-removal, power-loss resilience or new hardware drivers. V gates are **deferred**, not waived as passed. Optional legacy toolkit/workspace attachment is disabled by default in this profile; existing files and the old rescue image are preserved.

## Decisions

### D1. Keep boot components separate; freeze the known kernel lane

```
VitaOS -> model-correct loader -> pinned Vita kernel + embedded Buildroot /init
                                      |
                        select before normal services
                          /                         \
                    rescue init              approved USB container
                    no auto-mount            -> ext4 loop root -> sysvinit
```

Buildroot supplies rescue/early userspace; Debian supplies normal `/usr`, package databases, services and persistent identity. No Debian libraries are installed into Buildroot and no stock Debian kernel replaces the Vita kernel. The initial baseline pins kernel `37b9348710dfe1751dae0ef0fd2954b2714d08d4`, Buildroot `52ee2f5644da5389634b2465f1dc31deb5a4807c` and loader `96c225f1192d22d43064308be8d7aedb0827d036` as observed Git inputs; loader **deployed artifact** identity still needs independent capture. Pin changes require a separately reviewed change.

A changed `/init` changes the embedded initramfs and therefore the zImage even if kernel source stays pinned. Match the built kernel's extracted initramfs to the intended source cpio and inspect its files before hardware authorization. A successful `make dtb` is not that gate. Reboot returns through VitaOS and an explicit loader launch; automatic power-on Linux is not designed here.

**Alternative rejected:** replace kernel/loader or pursue native partitions concurrently; this would change too many independently failing layers.

### D2. Reproducible assembly, not a manually inherited rootfs

Add the recipe under existing `debian/`, not the old roadmap's absent `system/` tree. Use explicit foreign debootstrap plus QEMU second stage on a Linux host, with service starts suppressed during assembly. Pin bookworm/armhf, archive/keyring and signed repository metadata, exact package versions and package hashes in a machine-readable lock. Check the selected suite's security/support horizon when locking; do not silently upgrade to trixie if inputs expire or disappear. An unavailable or unauthenticated lock blocks the build with a decision record.

The builder accepts a fresh output directory and an explicit lock, refuses existing output, unsafe paths, wrong architecture, insufficient space and unpinned dependencies. Record debootstrap/QEMU/e2fsprogs/toolchain versions. Dependency installation is explicit, not a side effect of dry-run/validation. Use the existing `debian/packages.txt` as the seed, including network prerequisites absent from the early chroot trial. Package selection and lifecycle overrides must survive a repeat assembly from the same lock.

Create an 8 GiB ext4 image with eager inode/journal initialization on approved host scratch; regular files only, never raw devices. Any required formatter blocked by the execution environment is a human-run checkpoint, not bypassed. Account for apparent image size, two-build scratch and full allocation on the target exFAT. No assumption about current writable mounts/free space is inherited from old handoffs.

Separate public non-secret assembly from private enrollment of root media identity, authorized keys, Wi-Fi configuration and persistent SSH host identity. Preserve existing rescue identity privately; normal-root enrollment gets its own recorded fingerprint. Public outputs contain neither private keys nor user authorized-key lists/Wi-Fi secrets; firmware redistribution is not authorized by this milestone. The image inspector checks effective `sshd` policy, owner/mode, required firmware in both boot layers, service registration and `/run` recreation without printing secrets. Remove build-only service suppression before finalizing. Inspect filesystems offline, unmounted; never run `e2fsck` against the live root.

**Alternative rejected:** opaque copying of the current private image. It preserves a useful rollback but cannot establish reproducibility.

### D3. Enrollment is the root-selection trust anchor

Proposed `debian/root-contract.schema.json` describes a versioned, operator-reviewed enrollment record installed in the private rescue image: device model `pstv`, USB transport, outer filesystem UUID/type, safe relative image filename, image instance ID, ext4 UUID, architecture and intended init. The image carries the matching non-secret instance ID. The approved rescue image plus offline operator enrollment is the trust anchor; a label on arbitrary media is not authorization. This is not secure boot against malicious local storage or privileged attackers.

Use sysfs transport identity and UUID/content checks rather than `sda1` or `mmcblkN`. Require one match, reject duplicates/clones, missing/malformed enrollment, internal MMC and unsupported types. No automatic SD fallback in the foundation profile. Existing compatibility behavior can remain outside this profile but cannot satisfy foundation acceptance. Do not use a mutable image's original SHA-256 as a permanent boot test: transfer hash checks precede first mutation; subsequent boots validate enrollment/instance/filesystem identity and state.

Discover/mount outer media read-only first, then follow one explicit order: read-only discovery, identity and health check -> controlled remount of the enrolled outer volume read-write -> durable attempt record with readback -> non-mutating ext4 admission of the inner image -> checked handoff. No write to any medium is permitted before that controlled remount, and it is the only read-write transition this design grants. Current `/init` mounts the container rw before selection, so this is a sequencing change rather than a wording one. Explicit rescue override bypasses image execution, repair and write mounts. A trial is admitted only after a current offline read-only outer-media health gate and backup/restore checks. Before rw admission use non-mutating ext4 checks on the unmounted candidate (`e2fsck -fn`); any nonzero/indeterminate result, missing tool or dirty image goes to rescue. **No automatic `-p` repair or journal-replay write is implicit in this design.** Any repair is a separate human-authorized recovery action. Allocate a free loop device; never pre-detach a guessed loop number. Carry the outer mount and `/dev` into the normal root as one checked handoff; track only resources this attempt owns. Failed cleanup prevents further automatic mounts and produces a visible degraded-rescue reason.

Foundation rescue disables the old automatic `S06toolkit` waiter and any competing USB/storage auto-mounter identified by a packed-image service audit. It must not mount an unrelated SD card when USB selection fails. Manual read-only diagnostics remain possible; legacy workbench files are not moved or deleted.

**Alternative rejected:** labels alone, first eligible FAT partition, automatic repair, or inheriting a hash as immutable identity of a writable root.

### D4. Correlated boot attempts and conservative recovery

Extend the existing selector/confirmation protocol rather than adding a second boot owner. A versioned attempt record binds candidate ID, attempt ID, consecutive-failure count and phase. Timestamp is diagnostic; clock jumps do not decide recovery. Persist `opened` successfully before handoff. Publish a durable usable confirmation only for that attempt, after intended PID 1, rw root/container, non-loopback IPv4/default route and SSH listener are checked. Confirmed boot is not itself proof of successful external authentication or package persistence; P gates add those checks.

Partial, unknown-version, mismatched, unreadable or corrupt state fails closed. A write/sync failure cannot manufacture successful confirmation; use a pending record and readback/commit discipline so ambiguous state selects rescue. Three consecutive unconfirmed attempts select rescue on the next loader boot. An intervening valid confirmation resets that streak. Boot-loop prevention is a next-boot decision, not a watchdog or automatic reboot of a hung PID 1.

Retain operator `rescue`, one-shot `normal`, and `reset`, including CRLF support. Empty/unreadable/unknown override is not the same as absent. `normal` bypasses only retry limits, never identity/health/durability safety checks. `reset` preserves the previous corrupt record in recovery evidence, then initializes valid state. Legacy unversioned history selects rescue until explicitly enrolled/reset; do not erase it silently. Rescue override must be usable from a separate host with Linux stopped; VitaOS USB write access is optional until separately proven.

### D5. A narrow, observable graceful-shutdown contract

This prototype does **not** claim it can unmount `/`, detach its loop and unmount exFAT while executing from that same root. Preserve the later E4 approach: stop dependents; release owned optional mounts/non-root loops; remount root read-only while outer remains writable; flush the outer volume; persist evidence; then terminal action. Root loop remains attached. Full return-to-initramfs teardown is a later design.

Generate and inspect both rc0 and rc6 ordering using the actual locked Debian packages. Header text and hand-chosen K numbers are insufficient. Make these ownership edges explicit:

```
stop users/services
 -> release owned optional mounts and non-root loops (verify actual state)
 -> root ro (verify actual state)
 -> backing volume rw + successful flush
 -> durable `backing-flushed` record for this boot
 -> remaining backing-store handling by the stock rc0/rc6 owners
 -> one aggregate shutdown result published for the terminal boundary
 -> guarded halt/reboot
```

`backing-flushed` is deliberately **not** named shutdown-complete. It is published *before* the stock late handling runs, so no record may let a later failure read as success; the terminal boundary consumes the aggregate result, not the flush record.

A terminal interlock is **new required work**, not supplied by today's nonzero script exits. The **single interception point** is the rc0/rc6 terminal action itself: recipe-owned `dpkg-divert` entries wrap the stock `/sbin/halt` and `/sbin/reboot`, plus only those `sendsigs`/`umountfs` bodies the audited caller graph actually requires, so any path that reaches a terminal command passes through the guard whichever earlier script failed. rc0 and rc6 are guarded separately with their own recorded precondition. One **aggregate shutdown-result producer** is authoritative for the guard: it enumerates and verifies every owned stage (optional-mount unmount, non-root-loop detach, mount-state verification, root read-only, backing flush) and publishes a single result. `vita-card-mounts` must stop returning success after a logged unmount failure, and stock late handling after `vita-root-sync` must not be able to publish success on its own. Prepare the runtime result channel in ephemeral `/run`; its default is pending/failure. The terminal hooks invoke the real halt/reboot only after all earlier owned steps and final backing-store handling report success. Missing channel, failed sync/unmount/enumeration, unchanged mount state or unexpected graph blocks automatic terminal action and reports the failure on HDMI/local console. Hook restoration is part of recipe uninstall/rollback tests. Forced kernel reset/power loss is outside this cooperative contract.

Persist a shutdown-pending phase **before** stopping writers, on the backing medium and not only in `/run`. The durable record is a versioned three-state machine — `pending` -> `backing-flushed` -> `terminal-requested` — consumed and cleared by the next boot; because a terminal command does not return, `terminal-requested` certifies only that the terminal action was *authorized*, and only next-boot consumption plus observed mount/media state can establish what actually followed. A previously confirmed boot found in `pending` or `terminal-requested` must choose rescue on the next attempt. Do not trust absence of an `unclean-shutdown` line as proof. If the required state cannot be written or synced to storage, the design must not pretend a rescue decision was durably recorded: the terminal action is blocked fail-closed with a console fault, and the next boot is recorded as **unknown** and treated as unconfirmed. A `shutdown-synced` record certifies only root-ro/backing-flushed, never full detach or clean outer unmount. If final handling cannot meet the declared contract on the pinned system, the hardware milestone is blocked, not redefined as passed.

**Alternative rejected:** interpreting a stop script's exit as proof SysV stopped the shutdown, or claiming ext4 journal presence makes exFAT power-loss-safe.

### D6. Acceptance uses a staged evidence ladder

All gate IDs below are **planned / not run in this planning session**.

- **H1 host safety:** nonprivileged fixtures for recipe refusal, root selection/identity, duplicate media, corrupt records, override semantics, sync failures, occupied loops and cleanup failures. Fake commands assert no real device/network operations. Existing regressions stay in `make test`.
- **I1 locked build:** two fresh assemblies from one lock; compare package/version inventory and normalized non-secret configuration, document expected UUID/key/timestamp variability; build rescue/zImage with canonical pins; inspect embedded initramfs, model DTB, required tools, architecture/init, provisioning separation and manifest hashes.
- **I2 lifecycle image:** run actual insserv in the assembled root, reject cycles, inspect startup and rc0/rc6 graphs; execute graph-shaped failure fixtures with terminal commands stubbed. Run privileged mount tests only on disposable host image files in an explicitly approved environment, never on untrusted CI contributions or real media.
- **P0 authorization/preflight:** assigned hardware owner and Cassie-present window; exact candidate/rollback manifests; backed-up media, proven restore on disposable media, offline read-only filesystem health and measured free space. Verify rescue reachability before normal-root trials. This is a hard stop until approved.
- **P1 boot/recovery:** explicit rescue override; enrolled USB absent while SD remains untouched; invalid-root fixture; valid root with native PID1/local console/key-auth SSH. Corrupt history and simulated exhausted attempts must select rescue. Actual post-handoff non-confirmation uses an approved disposable trial copy, not a destructive PID1 panic; verify the next-boot policy. Record separate boot IDs and allow a bounded 420-second observation window before classifying a timeout as inconclusive, with HDMI diagnosis.
- **P2 package persistence:** authenticated apt update, install/run, an actual version-changing upgrade and removal with `dpkg --audit`, package/index/keyring provenance and maintainer-script/service outcomes. A no-op upgrade is not a pass. Save package database/version inventory, edited config, home marker and a compiled C/pthreads program checksum. Repeat checks after graceful restart through VitaOS/loader. Capture at least two distinct successful restart transitions, then one orderly poweroff/cold start; no dirty power pull.
- **P3 lifecycle:** observe shutdown phase ordering on the exact package/kernel pair, terminal return/poweroff, next-boot records and offline post-shutdown `e2fsck -fn` plus `fsck.exfat -n` **before Linux writes to the medium again**. Use an external host/reader and quiesced media; inability to obtain this evidence is a blocked gate. Safe synthetic failure injection first uses H/I; P deliberately avoids inducing filesystem corruption. The trial does not prove sudden-power-loss safety.
- **P4 protected baseline (two tiers):** required core regressions for SMP, framebuffer/local USB keyboard input, Wi-Fi/key-auth SSH and USB-root operation; and a separately reported compatibility result for previously validated Bluetooth controller operation plus pairing persistence. An absent required keyboard or network path blocks the core component; an absent controller blocks only the compatibility result and is recorded blocked, never failed and never as evidence that Bluetooth HID never worked. Wired link integration and optional toolkit restoration remain deferred.
- **V1:** future Vita 1000 storage/model inventory followed by its own native/boot/lifecycle gates; Vita 2000 requires its own hardware. Nothing in this change runs V1 or claims parity.

### D7. Evidence stays owned by the integration project

New integration results go to N:`lab/pstv-foundation/<run-id>/` with commands, outputs/exit codes, hashes, model, firmware, boot IDs, timestamps, expected/observed signals, timeout, rollback disposition and `what-this-does-not-prove`. `docs/evidence/pstv-foundation.md` becomes the claim index during implementation. R remains read-only: link its immutable commit/path evidence rather than changing historical records or creating an OpenSpec root there.

Classify claims **verified**, **directly observed**, **corroborated**, **inferred**, **candidate**, **unknown**, **deferred**, **hardware-gated**, or **host-verifiable**. Stored hardware outputs are historical measurements, never live-device inventory. Negative/inconclusive results are publishable, but do not satisfy positive acceptance. Record credential presence/modes/fingerprints only; no secrets in logs or public artifacts. Optional outputs and package availability cannot silently narrow required acceptance.

## Risks / Trade-offs

- [Nested durability remains limited] -> retain backups, no hot-removal or dirty power tests, distinguish flush from full unmount, require offline health checks. Native partition migration stays separate.
- [Late shutdown executes from a readonly root] -> stage runtime state in `/run`, verify executable dependencies and both terminal graphs in I2 before P authorization; fail closed if the mechanism cannot be exercised.
- [Package updates regenerate service graphs] -> lock initial versions and rerun graph/interlock checks after the native upgrade test; stop on cycles or moved terminal ordering.
- [USB UUID clones or removable-media replacement] -> reject ambiguity, bind outer and inner identity to operator enrollment; re-enrollment is explicit, not autodetection.
- [Rescue falls into legacy auto-mounts] -> foundation profile suppresses automatic Workbench/storage ownership; verify the packed service set, not only `/init`.
- [Bookworm inputs age] -> capture support/source availability at lock time; a suite transition is a new reviewed decision, not hidden fallback.
- [Long integrity/boot checks time out] -> size budgets explicitly; completed whole-image hash is required; aborted checks are inconclusive. Historical 22-minute hash is a budgeting hint, not a future performance guarantee.
- [Preservation versus convenience] -> optional workspace/toolkit stays disabled by default; no existing files are reused as scratch. A lost convenience mount is documented rather than pretending it is a journalled workspace.

## Migration Plan

1. Implement H and I work only after a new implementation request. Preserve current branch content and dated records. No device work is necessary to write/test these contracts.
2. Produce a frozen public/private-classified candidate set and operator runbook. Keep original mutable root, card archives, known-good rescue kernel and loader assets untouched. Do not overwrite `workspace.ext4`, `workspace.ext2.old` or old toolkit bundles.
3. Stop for P0 authorization. Stage to a distinct path on approved media using partial-file transfer plus whole-file hash/readback before activation; no overwrite of the old active root. Enroll its distinct root filename/identity in the paired rescue candidate. Take another manifest after provisioning, before transfer.
4. Run rescue first, then the bounded P gates, one hardware owner at a time. An unexplained storage/input/network fault, mismatch, unsafe cleanup or timed-out check stops progression.
5. Recovery: halt further writes, inspect console evidence, use the external-host override with Linux stopped; boot pinned rescue. Kernel/loader failure requires the documented VitaOS recovery window and readback-verified rollback artifacts. Removing the USB is allowed only while powered off or after proven safe release, never while it backs `/`.
6. Rollback preserves failed-candidate evidence; no repair, reformat or deletion without separate approval. Demonstrate rollback on hardware before declaring the foundation accepted. Update README/status with dated, bounded claims only after those gates—not because this artifact graph is complete.

## Open Questions

No unresolved architecture choice blocks host implementation. These are operator selections or measurements with fixed refusal behavior, not permission to choose a weaker scope:

- Which physical USB medium, backup destination and disposable restore-test medium will Cassie authorize for P0? No device write until selected, healthy and restore-tested.
- When is the supervised PSTV window, with HDMI, USB keyboard, previously validated Bluetooth controller and a host/reader for offline checks available? Missing equipment blocks its gate.
- What signed repository snapshot/package-version pair is available for a real upgrade when implementation starts? The lock/audit task must pin it and demonstrate a changed installed version; unavailable inputs block P2 rather than accepting a no-op.
- Whether VitaOS can write this USB is unknown and not required: the external-host override is the selected recovery method. Whether full teardown or a native partition should follow belongs to a later change, after this foundation's outcome is recorded.

## Deferred roadmap note — clock-source discipline (Vita-scoped, not this milestone)

`periphclk` is a corrected boot-configuration model, not a disciplined time source (E8). This note adds no task, gate or tolerance to this change: nothing in `tasks.md` depends on resolving it, and duration-based acceptance is already budgeted with ~±0.25% worst-case headroom. It becomes load-bearing only for future Vita-scoped work, and should be inherited explicitly rather than rediscovered:

- Read and record each model's clock-generator register 2 at boot as evidence rather than inference. The handhelds' effective SSC state has never been read, and all three DTBs inherit one shared constant.
- Resolve the latch-at-power-up versus write-protected-byte-2 alternatives with an I²C capture of the `0x82` transaction or a controlled reg2 write-and-read-back. Either would also settle whether a firmware SSC write can ever move the effective timekeeping rate by ~2,500 ppm.
- Decide whether time should be disciplined from the RTC/NTP rather than trusted from a DT constant, and whether any future CPU selector/divisor change invalidates the constant outright.

Owner and venue: the deferred V1 Vita inventory in D6, with its own dated hardware record. None of this is a candidate for acceptance in this milestone.
