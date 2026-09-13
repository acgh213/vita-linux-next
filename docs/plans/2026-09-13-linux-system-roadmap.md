# PSTV-first packaged Linux implementation plan

> **For Hermes:** Use subagent-driven-development skill to implement this plan task-by-task.

**Goal:** A recoverable PSTV Linux installation with a persistent, package-managed
root, useful native programs, and an explicit path to model-specific Vita parity.

**Architecture:** Preserve the working custom kernel and rescue initramfs. Prove
Debian armhf natively in a separate ext4 image on the existing exFAT SD2Vita, then
implement early root selection and normal distribution services. Neither a chroot
nor a successful SSH connection is a complete boot/recovery acceptance test.

**Tech Stack:** Linux 6.12 Vita fork, Buildroot rescue, Debian 13 armhf first candidate,
ext4 loop over exFAT, distro apt/dpkg, shell/Python host fixtures, native ARM tests.

**Status:** Plan/research only. User selected general-purpose repositories/native
development and preserving exFAT/data. Debian support on this device is a hypothesis
with a gate. No new device deployment, repartitioning or repair in this planning PR.

**User decisions recorded 2026-09-13 (these are settled; do not re-litigate in a PR):**

1. **Debian is the primary userland.** The user's words on musl: *"makes it weird."*
2. **Alpine armv7 is a welcome second target, not a fallback** (**#35**). The user explicitly
   likes Alpine. Sequencing puts Debian first because glibc answers more questions per
   hour; Alpine's gates are package coverage and genuine glibc dependence, and it may
   pass or fail those on its own merits. Do not write Alpine off as a rejection.
| 3 | **A Linux-native partition is the intended end state** (#34 / Lane H), not a nice-to-have.
   Nothing repartitions until the backup/rollback and VitaOS `ux0:` compatibility
   gates pass.
4. **Trial root image is 32 GiB**, fully allocated. Rationale below.
5. **Vita 1000 is online and available** (VitaOS; 1337/1338 open, 22 closed at
   192.168.18.36), so handheld lanes are not hardware-blocked.

### Why 32 GiB rather than 16 GiB

The card has **111.2 GiB free** (119.4 GiB total, 8.1 GiB used as of 2026-09-13), so
the choice is about headroom, not scarcity. Sizing is dominated by native development,
not by the OS: a Debian armhf base plus GCC/G++/git/python3 is roughly 2.5–3.5 GB
installed, but a kernel build tree is another ~1.5–2 GB, `ccache` is worth 2–10 GB if
enabled, Python venvs and package caches are 1–3 GB, and distro upgrades keep old
package versions around. 16 GiB works for the system and then starts pinching on the
first serious build — which is exactly the workload this whole effort exists to
support. 32 GiB leaves ~79 GiB on the card and is the size at which a build failure is
unlikely to be "out of disk." **Fully allocated, not sparse:** exFAT sparse behavior
is not something to rely on for a root filesystem.

---

## Read this first

- [Architecture and failure contract](../architecture/linux-system.md).
- [Fresh baseline and evidence corrections](../evidence/2026-09-13-system-audit.md).
- Existing [workbench design](2026-09-07-vita-linux-workbench.md),
  [project status](../PROJECT-STATUS.md), and [hardware roadmap](../HARDWARE-ROADMAP.md)
  contain historical measurements, not automatic claims about the next image.
- Parent tracker: [#18](https://github.com/acgh213/vita-linux-next/issues/18).
- Kernel integration: `acgh213/linux_vita:vita-linux-next`, baseline
  `37b9348710dfe1751dae0ef0fd2954b2714d08d4`; outer reviewed base
  `2820f9558219abca180c923cbc591140afd98b61` (includes exfatprogs selection,
  NOT a tested rootfs build). Re-read live refs before starting; never reset to
  these merely because they are printed here.

## Finish line and non-goals

Done means: normal Linux PID1/local login/authorized SSH; signed-repository install,
upgrade and removal; package database/config/home/project persistence through a
graceful reboot; GCC/C++ and a real small program build; an explicit tested rescue
path; orderly nested-filesystem shutdown; reproducible non-secret release inputs;
model-specific evidence. A supported PSTV release does not imply Vita support.

Not required: replacing VitaOS, direct power-on Linux, SGX acceleration, a large
desktop/browser, native LLVM/Rust builds, CP14 debug, arbitrary raw hardware-control
APIs, unreviewed suspend, or repartitioning the user's card.

## Dependency map

```
A0 evidence/rollback/build inventory (#13) -----+------------------------------+
A1 storage lifecycle/identity (#14) ----------+--> B native distro trial (#27) |
A2 SDIF1 observational baseline (#19) --------+          |                    |
A3 safe reset ownership (#16) --------------------------+--> C boot (#28)     |
                                                        |         |          |
D identity/services (#29): fixtures early, native after B/C -------+          |
E programs (#30): fixtures early, chroot tiers after B; reboot after C/D       |
USB (#3), display (#8), audio (#5), dashboard (#4): independent HW lanes ------+
                                                        |
                                    F release (#13) -> G handheld (#31)

C exit criteria ---> H native partition migration (user-stated destination)

```

Lane H (native partition) is gated on C meeting its exit criteria, because a native
partition is only worth having once something reliable boots from a nested image.
#17 settle-delay tuning follows #19 and safe #16 experiments, not the critical
path. #32 opkg config repair is useful parallel rescue maintenance, not a
prerequisite to the normal-distro path. Read-only observations can start before
reset controls are refactored; no destructive reset/hotplug gate on mounted root.

## Agent execution contract (every PR)

1. Read this plan, linked issue body/comments and exact source at current base.
   Capture `git status --short`, branch/ref and artifact identities. Claim a lane
   in its issue before modifying shared files; only one agent owns the PSTV.
2. Create an isolated topic worktree from canonical base. Do not clone/copy secrets
   into a new tree without the established private build/provisioning procedure.
3. For each code change, write a failing fixture, run it (record actual failure),
   implement the smallest change, rerun, then broader relevant regression tests.
4. Stage named paths only; run diff/secret checks, commit, request independent
   review. A kernel change is a kernel PR plus an outer pin PR; do not stack other
   features onto an unreviewed hardware candidate.
5. Label every result host-only / emulator / native userspace / hardware boot.
   CI currently checks contracts/cart/DTBs, not a complete rootfs or hardware boot.
6. Stop at the physical boundary. Deploy/reboot/repair/format/network mutation
   requires an agreed window with Cassie and exact rollback. No raw syscon/SMC,
   no host network readdressing, no internal MMC/VitaOS write mounts.
7. Leave a resume record: commit/PR, files changed, tests and outputs, actual
   artifacts, what remains untested, next command, blockers, hardware ownership.

All `system/` paths below are **new proposed files**, not commands already present.
Tests named below must be created and exercised before claiming that milestone.

## Adversarial review findings (2026-09-13) — status: DOCUMENTATION APPROVED ONLY

An independent reviewer (`lab/system-roadmap-audit-2026-09-13/plan-review.md`)
reviewed this plan with no author context. Verdict: **the architecture has the right
safety intent but is not yet an executable or hardware-authorizable normal-root
plan.** Two P0 findings and three P1 findings are folded into the lanes below.
**Implementation and any hardware trial stay blocked on the two P0 items.**

| # | Sev | Finding | Where it is now addressed |
|---|---|---|---|
| 1 | **P0** | Normal-root handoff has no *implemented* recovery mechanism: no boot-attempt marker, no rescue-visible state, no operator override, no next-boot decision. A backup filename is not rollback. | Lane C step 5 + "Boot-attempt record" below |
| 2 | **P0** | Shutdown is described but not *connected*: the real distro PID1 is unnamed, `S06toolkit`'s waiter can race a stop and remount storage, and the proposed shutdown script is not shown to be invoked by anything. | Lanes B step 3 and C step 3; A1 step 5 |
| 3 | P1 | Rootfs/initramfs prerequisites incomplete: bootstrap method, service-start suppression, PID1 choice, and the rescue tool/kernel capability list are all unspecified. | Lane B steps 3 and 5 |
| 4 | P1 | Mutable-root identity underspecified: a content hash cannot be a permanent runtime identity for an image that changes on install; marker schema, trust anchor, duplicate/stale/partial images undefined. | Lane A0 step 3 and A1 step 2 |
| 5 | P1 | Package acceptance misses the stated finish line: no upgrade transaction, no maintainer-script/restart behavior, no provenance record. | Lane B step 5, Lane E |

### Boot-attempt record (the P0 mechanism, to be implemented and tested in C)

`switch_root` **execs** the new PID 1. If that exec or the distro init then fails, the
kernel panics and *nothing in the old initramfs resumes* — architecture
`linux-system.md` already says this. Recovery therefore **cannot** be reactive. It has
to be a decision made *before* the handoff, from state the previous attempt left behind:

1. **Before** handing off, rescue writes/updates a boot-attempt record on the exFAT
   transport (a small file with attempt number, candidate identity, timestamp, and the
   reason for the attempt).
2. The normal root's **earliest** successful service clears the record (that clear is
   the proof of a good boot — not a login, not an SSH connection).
3. On the next boot, if the attempt count exceeds a small bound, rescue **selects
   rescue by default** and reports the reason. It never silently retries forever, and
   never auto-selects an unverified alternate image.
4. A **human override** must exist that forces rescue without editing anything on the
   normal root — reachable from the loader/console side, since Wi-Fi/SSH may be part of
   what is broken.
5. The record write path must be tested independently: partially written, stale,
   unwritable-media, clock-jump, and "record says attempt 3 but the root is fine" cases.

Until this mechanism exists and its tests pass, **no normal-root boot may be deployed**,
because the only recovery would be pulling the card.


## A0 — Evidence, rollback and reproducible build foundation (#13)

**Objective:** The next agent can build and recover without this machine's temporary
worktrees or private chat notes. **Parallel:** all other host-only lanes.

**Files:** create `system/artifacts/schema.json`, `system/artifacts/verify.py`,
`system/tests/test-artifacts.py`; modify `Makefile`, `README.md` and
`.github/workflows/build.yml` in separate narrowly reviewed build PRs as needed.

Small steps:
1. Write fixtures for wrong file size/hash, missing target DTB, secret-bearing
   public artifact, stale gitlink and misleading filename. Start with a synthetic
   non-secret fixture; never distribute the real private rollback images in tests.
2. Run `python3 -m unittest discover -s system/tests -p 'test-artifacts.py'`;
   expected RED until implementation exists. Implement content-based identity
   verification, then expect GREEN (record actual counts, never copy this prose).
3. Verify the local rollback mismatch: `zImage.proven-24911272B` is actually
   24,765,560 bytes, hash `8f9b1a82...`; do not relabel it as the P1 image.
   Identify embedded release before use. Preserve originals and private modes.
4. Fix fresh-checkout build/bootstrap provenance: use canonical submodule gitlinks,
   no inherited remote/branch defaults or hidden `.linux-vita-dir` dependency.
   Record external toolchain/rootfs/DTB/loader hashes and exact config.
5. Add a Linux full-rootfs CI/artifact gate distinct from current DTB smoketests,
   with disk/cache budgeting and explicit absence of private provisioning data.
   Verify exfatprogs in the packed artifact and run its binary before claiming it.
6. Commit each independently tested unit. No cleaning shared builds merely to
   make the checkout appear clean; preserve current uncommitted work/artifacts.

**Exit:** verified rollback manifest; clean-checkout documented build path; honest
CI boundary. Versioned release stays open until native acceptance below passes.

## A1 — Storage identity, health and nested mount lifecycle (#14)

**Files:** modify `buildroot-vita/board/vita/overlay/etc/init.d/S06toolkit`; create
`system/storage/identify-card.sh`, `system/tests/test-storage-lifecycle.sh` and
`docs/STORAGE-RECOVERY.md`. Keep scripts small and fake-root testable.

Small steps:
2. Fixture tests: numbering shifts, internal MMC with FAT, multiple SD candidates,
   absent/late/dirty media, malformed marker, background waiter cancelled during
   stop, failed/busy unmount, lost block device and missing payload hash. Define the
   **marker schema and its trust anchor** here: a **content hash is not a permanent
   runtime identity for a root that changes when packages are installed.** Separate
   *signed/immutable selection metadata* (what we intend to boot) from *mutable image
   state* (what it has become), and test ambiguous, stale, replaced, cloned and
   partially written images.
2. Run `sh system/tests/test-storage-lifecycle.sh` (RED), implement identity based
   on SDIF/sysfs + explicit filesystem/marker selection, then rerun (GREEN).
3. Ensure no fallback write into bare RAM mountpoint and no RW mount of an unknown
   FAT partition. Make service dependencies/readiness states explicit.
4. Give waiter a tracked lifetime, lock start/stop against it, cancel/join before
   shutdown. Order dependents -> ext4 -> squashfs -> loops -> outer exFAT.
   Emit failure and refuse unsafe next steps if a layer remains busy.
5. Make artifact verification fail closed for missing/invalid expected hash;
   preserve a separately declared diagnostic/unverified mode if needed.
6. Before the first new trial image: inventory and back up current user data,
   verify restoration on disposable media, measure real free space. Check exFAT
   only after consumers/loops are stopped and the outer filesystem is unmounted.
   `fsck.exfat -n` is diagnostic, not a repair; handle each exit status. Obtain
   separate approval for a repair if required. No resizing mounted production image.
   **This is a hard prerequisite, not a planned step:** the captured card reported an
   unclean exFAT unmount (`lab/system-roadmap-audit-2026-09-13/live-public.txt`), so a
   card-health gate with an **actual read-only result** must be recorded before any
   trial root is staged.

**Native exit:** unchanged data; identity correct; approved diagnostic image mount,
metadata and clean unmount pass; stop-during-wait cannot remount storage. Explicitly
record that ext4-on-exFAT is not a complete power-loss guarantee.

## A2 — Explain SDIF1 delays before tuning (#19)

**Files:** create `system/probes/storage-baseline.sh`,
`system/tests/test-storage-report.sh`; instrument existing kernel
`drivers/mmc/host/sdhci-vita.c` and standard MMC tracepoints only as justified.

Small steps:
1. Create fixture log containing probe `[no card]`, later successful enumeration,
   delayed discovery and unrelated Wi-Fi MMC traffic. The parser must distinguish
   rail/host/card/partition/filesystem/service readiness and identify the right host.
2. Run fixture (RED), implement parser (GREEN). Persist whole relevant lifecycle,
   uptime plus boot ID, preceding shutdown condition, artifact manifest and waiter
   timestamps; do not infer independent boots from wall-clock/timezone differences.
3. First native observation is seated/untouched card, no reset or rail writes.
   Use bounded read-only file samples and report timeout/error counters before/after.
4. After consent, compare controlled warm/cold boot samples without replugging;
   correlate MMC detect-worker entry, commands and responses with userspace polling.
   Missing timeout printk does not establish absence of silent retries.
5. Propose a fix only from an isolated mechanism. A 240-second waiter is an
   operational timeout, not a proven worst-case bound.

**Exit:** a reproducible explanation or a clearly bounded dataset and next probe;
no invented minimum-delay optimum. Tune #17 only after evidence supports it.

## A3 — Safe host ownership, reset and removal (#16)

**Files:** kernel `drivers/mmc/host/sdhci-vita.c`, `drivers/mfd/vita-syscon.c`,
`include/linux/mfd/vita-syscon.h`; tests adjacent to driver or existing fake
transport harness after source review.

Small steps:
1. Map IRQ, MMC request/rescan, runtime-PM, driver-remove and syscon publication
   ownership. Define admissible states before exposing a second write lever.
2. Test invalid/busy/active-I/O states and failure unwinding first. Read-only state
   reporting is separate from mutating reset controls.
3. Keep rail-off `-EBUSY`. Make rail-only enable and rescan/reset semantics explicit;
   do not race active requests or assume 'no direct sector write' means safe.
4. Review mixed line endings and exported symbol linkage; W=1 builds compared
   against baseline. Hardware mutation only on unmounted disposable/approved media.
5. Kernel review -> kernel PR -> outer pin PR -> approved hardware gate, never
   merge before the independent review finishes.

**Exit:** state/ownership contract and safe refusal/recovery evidence. SD2Vita
used as root is not hot-removable; a helper must refuse, not reset underneath it.

## B — Native Debian armhf feasibility (#27)

**Files:** `system/debian/build-rootfs.sh`, `system/debian/packages-base.txt`,
`system/debian/README.md`, `system/probes/userland-probe.sh`,
`system/tests/test-rootfs-recipe.sh`.

Small steps:
1. Pin Debian suite `trixie`, architecture `armhf`, repository/keyring and package
   input identity. Check support/security horizon at execution time. A stock Debian
   kernel/installer is not a Vita kernel. Build the root host-side with a documented
   foreign bootstrap/second-stage method; suppress package service starts.
2. Test recipe refuses wrong architecture, unsigned input, out-of-space and existing
   output destination. Run `sh system/tests/test-rootfs-recipe.sh` RED/GREEN.
3. Capture the *resolved* kernel config and compare chosen PID1 requirements:
   cgroups, namespaces, devpts, proc/sys, fhandle, seccomp, etc. Never enable every
   generic distro kernel option or CP14 just to satisfy a checklist. **Choose and
   record the PID1 in this step**, and write a **rescue capability manifest** naming
   every tool and kernel option the initramfs needs (exFAT, ext4, loop, `blkid`,
   hashing, `fsck.exfat -n`, mount moves, rescue shell). A chroot can pass while
   PID1, cgroups, devpts, udev, `/run`, networking or shutdown all fail — so state
   the bootstrap/second-stage method and service-start suppression explicitly.
4. Once A1's write gate passes, stage a separately allocated **32 GiB, fully
   allocated** ext4 image (see the sizing rationale above). Record hash/manifest prior
   to trial; no edits to `workspace.ext4` or old bundles. Leave `workspace.ext2.old`
   alone — reclaiming that 4 GiB archive is a separate, user-approved cleanup, not a
   step in this lane.
5. In a supervised trusted chroot, no new PID1 or network changes: verify shell,
   libc/ELF ABI, `dpkg --print-architecture`, signed `apt-get update`, a small
   dependency install/list/remove, **an upgrade transaction**, Python and minimal
   C/C++. Native execution, not host QEMU, is the acceptance. Chroot is not
   hostile-code containment. A chroot cannot prove PID1, cgroups, devpts, udev,
   `/run`, networking or shutdown. **Record exact repository, keyring and index
   provenance for every package version**, and check maintainer-script/restart
   behaviour rather than assuming dpkg did the right thing silently. Reboot
   persistence is proven in C/D, not here.
6. Tear down completely, remount, check DB/files, preserve rescue access. Document
   image size, RSS/time and logs. On failure, prefer the smallest specific fix; then
   hand the question to the **Alpine lane as a genuine second target** rather than
   parking it as a rejected comparison. Do not silently mix distro libraries into
   Buildroot `/` — that is the one outcome that makes a later failure undiagnosable.

**Exit:** real userland/package gate, NOT distro boot. Store exact package versions
and inputs; then select PID1 based on its requirements rather than a hunch.

## C — Early normal-root boot and recovery (#28)

**Files:** `system/initramfs/init`, `system/initramfs/select-root.sh`,
`system/initramfs/shutdown`, `system/tests/test-root-selection.sh`,
`buildroot-vita/board/vita/post_build.sh` and a separate tested rescue profile.

Small steps:
1. Write a pure selection fixture before mount logic. Example acceptance fixture
   interface (new script must be implemented, not an existing command):
   `sh system/tests/test-root-selection.sh` covers override/missing/ambiguous/
   wrong-hash/wrong-ABI/timeout/invalid-init and expects explicit `rescue:<reason>`.
2. Implement state machine and failure unwinding; test all routes GREEN. Do not
   run S06 background mounting and normal-root ownership concurrently.
3. In host Linux integration tests, preserve outer exFAT + loop lifetime while
   moving mounts to new root; test orderly shutdown from code that is not on
   the filesystem it must unmount. No privileged CI mount test on untrusted PRs.
4. Add the smallest independently reviewed kernel config delta required by the
   selected distro init. Use a separate kernel PR and tested outer pin.
5. Implement the **boot-attempt record** above before any trial handoff, plus explicit
   rescue selection and an operator override that does not require the normal root to
   work. Test recovery after a bad PID1, not merely pre-exec mount failures. **Name the
   distro PID1 explicitly and show how shutdown is invoked by it** — an uninvoked
   shutdown script is not a shutdown path. No automatic rollback claim based on having
   a backup filename; never auto-select an unverified alternate image. Scope the
   README's existing "clean reboot/poweroff" claim to *today's RAM-rescue baseline*,
   not to this new nested-root path, until this passes.
6. Supervised PSTV trials: no card -> rescue; invalid trial -> rescue; approved
   trial -> normal local login/SSH; graceful reboot retains package/config/program.
   No destructive production-card power cuts. Record stage timestamps separately
   from SSH polling latency.

**Exit:** normal boot + realistic recovery + clean shutdown. Keep rescue and old
workspace untouched until upgrade/migration acceptance explicitly permits change.

## D — Identity, services and daily writable state (#29)

**Files:** `system/provision/`, `system/services/`,
`system/tests/test-provisioning.sh`, `system/tests/test-service-state.sh`,
`docs/INSTALLING-LINUX.md`.

Small steps:
1. Fixtures for first enrollment, missing credentials, duplicate machine ID,
   wrong key ownership/mode, unavailable workspace and low-space log handling.
2. Test RED/GREEN, then implement distribution-native service ownership: network,
   SSH, Bluetooth, time, logging and clean stop. Do not run Buildroot rc scripts
   alongside distro services controlling the same hardware.
3. Provision unique identity locally/offline; preserve private rescue identity
   through transition. Daily user has a documented privilege path.
4. Native reboot verifies SSH host identity, BT pairing (without logging keys),
   home/config/service state. Bound logs/caches; ephemeral `/run` and `/tmp`.
5. Missing Wi-Fi configuration must fail closed, never associate an arbitrary
   open AP. Network mutation needs a recovery window, not an experiment by SSH.

**Exit:** reproducible personal enrollment and persistent service behavior. Public
images contain no real keys/credentials. Distribution support does not secure our
custom kernel automatically; assign maintenance and update policy under #13.

## E — Program tiers (#30)

**Files:** `system/packages/{base,developer,services,graphics}.txt`,
`system/tests/native-programs.sh`, `system/tests/fixtures/`, `docs/PROGRAMS.md`.

One PR per tier:
1. Base terminal/file/network utilities, using signed repositories; exercise a
   tmux session, editor write/reopen, git local clone, rsync comparison and HTTPS.
2. GCC/G++ headers and build tools: multi-file C/C++, pthreads/libm plus a small
   real upstream project. Start `-j1`; measure RSS/OOM before more parallelism.
3. Python venv + SQLite transaction/reopen + localhost HTTP service; managed
   restart/reboot after C/D. Go static CLI optional; native Go/Rust builds not a gate.
4. Software-rendered graphics and display cleanup; backend-specific SDL/KMS after
   #8, no assumed acceleration. Audio attaches to #5; absent hardware means
   blocked native gate, not fabricated pass.

Example C fixture body for the developer gate (compile inside distro, not Buildroot):

```c
#include <pthread.h>
#include <stdio.h>
static void *worker(void *p) { *(int *)p = 42; return 0; }
int main(void) {
    int result = 0; pthread_t t;
    if (pthread_create(&t, 0, worker, &result)) return 1;
    if (pthread_join(t, 0)) return 2;
    printf("worker=%d\n", result);
    return result == 42 ? 0 : 3;
}
```

Run in trial root: `cc -Wall -Wextra -pthread thread.c -o thread && ./thread`.
Expected output is `worker=42`; this is a planned assertion, not a reported result.
C++ and real upstream-build fixtures are required in addition to this tiny test.

**Exit:** useful applications with versioned acceptance and resource measurements;
file presence alone never closes a program tier.

## F — Release and supporting hardware lanes

- **#13 release:** documented clean build, signed/checksummed public artifact set,
  exact model/firmware prerequisites, first enrollment, install/upgrade/rollback
  gates. Catalog packages with versions and license/source provenance. Firmware
  redistribution rights must be reviewed; do not bundle arbitrary Sony dumps.
- **#3 USB:** preserve proven HID/EHCI first, finish attached-child teardown,
  reboot/VBUS ordering, repeated warm/cold boot, explicit suspend rejection.
- **#4 dashboard:** fixed payload, supervised input and display restoration,
  plus workspace persistence on exact deployed image.
- **#8 display:** standard DRM atomic/dumb-buffer client, process-death/VT cleanup;
  software rendering is useful before SGX. Do not claim compositor support until run.
- **#5 audio:** USB Audio Class descriptor/ALSA inventory, then approved bounded
  playback and cleanup. Native codec work remains separate.
- **#6/#7 PMU/CP14:** independent, not prerequisites for this roadmap. Preserve
  validated timer; do not reopen clock provenance.
- **#12 network control:** not a prerequisite; do not add remote mutation endpoints
  merely to make deployment easier.
- **#17 settle:** performance optimization only after safe measured experiments.
- **#32 opkg:** default config repair and persistent-destination smoke as a small
  rescue maintenance PR. No custom full package repository project by accident.

## G — Handheld rollout (#31, #9, #10, #11)

**Files:** `system/devices/{pstv,vita1000,vita2000}.json`,
`system/probes/device-profile.sh`, `system/tests/test-device-profile.sh`,
`docs/DEVICE-SUPPORT.md`.

1. Write fixture rejecting unknown model and preventing PSTV capabilities being
   inherited as true on handheld. Test RED/GREEN.
2. Read-only model/firmware/loader/storage/current-image inventory. Verify actual
   card availability and preserve backups before promising an SD2Vita root there.
3. Native distro chroot gate -> model-specific boot/recovery -> persistence and
   local display/buttons/touch/BT. Package recipes shared only where ABI holds.
4. Handheld external USB (#9) and read-only battery ABI (#11) are their own gates;
   no guessed charging/suspend/rail behavior. Vita 2000 support needs actual hardware.

**Exit:** a per-model capability matrix and evidence; PSTV release remains valid
if handheld support is still gated. No unsupported 'all Vita models' claims.

## H — Native Linux partition migration (#34) — user-stated destination

**Objective:** replace the loop-file-over-exFAT root with a real ext4 partition, removing
the nested filesystem, the loop device, and the unjournaled outer boundary in one move.
**Gate:** do not start before C meets its exit criteria. A native partition is only worth
having once something reliable boots from a nested image; doing it first means debugging
the partition and the boot at the same time.

**Files:** `system/storage/partition-layout.md`, `system/tests/test-partition-plan.sh`,
`docs/STORAGE-RECOVERY.md` updates, and a host-side `system/storage/backup-card.sh`.

Small steps:

1. **Backup before anything else, and prove the restore.** The existing
   2026-09-12 backup covers the exFAT *content*, not the partition table. A partition
   migration needs a whole-device image plus a tested restore on disposable media. Pick
   the destination medium for that image (server disk has ~9 GiB free; `disk1` has more)
   before starting, and verify the restore actually mounts.
2. **Answer the VitaOS question by test, not assumption.** VitaOS reads `ux0:` as a FAT
   volume. Whether it tolerates a FAT partition that does not span the whole card is
   **unknown**. Test on a disposable card first: partition it exFAT + ext4, confirm
   VitaOS still mounts `ux0:` and that Linux sees both. A card Linux likes and VitaOS
   cannot read is a regression. If it fails, the fallback is a **second card** rather
   than an exotic layout.
3. **Choose the layout explicitly, and record the loser.** Candidates: (a) one card,
   exFAT `ux0:` + ext4 Linux root; (b) two cards, one per OS; (c) **PSTV only:** root on
   USB, card untouched, since PSTV's EHCI host storage is already hardware-proven. Note
   that (c) does not exist for the handheld.
4. **Partition operations are a supervised, backed-up, user-present event.** No in-place
   resize of a mounted card, no repair command run blind, no "it is probably fine."
   Prepare exact commands and the rollback path in advance; if the rollback is not
   tested, the migration does not happen.
5. **Migrate the root and re-run the same gates as C,** not a shortcut subset: card
   identity, root selection, rescue entry, reboot persistence, and shutdown ordering
   (now simpler — one filesystem, but it still must flush and unmount cleanly).
6. **Keep the image-file path working.** Native partition is the destination; it is not
   grounds to delete the nested-image design before the replacement has survived real
   reboots. Retire the old path only on explicit user approval.

**Exit:** Linux boots from a native ext4 partition; VitaOS still reads `ux0:` (or the
two-card decision is recorded with the evidence); restore from backup demonstrated; the
image-file path retained or retired by explicit decision.

**Non-goals:** multi-boot cleverness, exotic partition schemes, encrypting a card whose
failure modes we have not yet characterized, or repartitioning the card the user is
currently using before the backup restore has been proven.

## Ready-to-dispatch lanes and handoff

Start **A0, A1 host fixtures, A2 read-only instrumentation design, B host recipe and
D enrollment fixtures** in isolated worktrees. They may research/code in parallel;
B native writes wait on A1/rollback approval. A3 mutating tests do not run against
an active root card. One hardware gate owner serializes all device operations.

The next agent's first action: claim #27 for the host-only Debian recipe, or #14
for storage lifecycle fixtures; read current issue comments and this plan. Do not
start by deploying a new kernel, installing all old `.ipk` files or repartitioning.

**Before dispatch, each lane must state its host dependencies, the exact bootstrap
command, the expected artifacts, and a finite stop/decision criterion.** "Explore
until something works" is not a lane. The Debian→Alpine fallback is **time-boxed**
with explicit reject/continue criteria so it cannot become another open-ended
architecture project. The resume template's blank "next exact command" must be filled
with a runnable command, not a description.

**Baseline vs goal, stated once so no agent conflates them:** today's hardware result
is *a 31,910,504-byte rescue/toolchain zImage that boots, with a RAM-only root.* That
is **not** a packaged-distro result. The finish line — persistent, signed-repository,
package-managed, survives-a-graceful-reboot Linux — does not exist yet on any device.

Resume record template (fill with actual results, never defaults):

```
Lane / issue / PR:
Base and candidate SHAs:
Host-only / native / boot gates actually run:
Artifact manifest and private/public classification:
Files and output paths:
Remaining unknowns / stop condition:
Hardware owner and Cassie-present window:
Next exact command:
```
