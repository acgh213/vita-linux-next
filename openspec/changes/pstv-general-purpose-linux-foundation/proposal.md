# Proposal

## Why

Vita Linux Next has progressed beyond the RAM-rescue baseline described in its top-level README: later September 13 records describe a PSTV Debian USB-root trial, and the inspected branch contains its boot and lifecycle integration. The next useful slice is to turn that one-device experiment into a reproducible, recoverable, explicitly gated foundation—not to repeat initial bring-up or declare the entire distribution finished.

## What Changes

**First milestone:** a reproducibly assembled Debian 12/bookworm armhf + sysvinit trial, in a separately allocated 8 GiB ext4 image on an explicitly enrolled PSTV USB exFAT volume, paired with the pinned custom kernel and Buildroot rescue initramfs. Completion requires independently recorded package/config/program persistence across supervised graceful restart, bounded rescue/recovery behavior, and observed shutdown ordering for the exact candidate. No deployment is authorized by this proposal.

- Codify and harden the existing root-selection, boot-attempt, mount-ownership and SysV lifecycle contracts rather than introducing a parallel `system/` implementation.
- Add a repository-owned, locked-input Debian assembly/inspection workflow, artifact provenance and separate private enrollment. Reproducible means repeatable inputs, package inventory and functional gates; byte-identical filesystem images are not promised.
- Require explicit USB/root identity, fail-closed discovery and record handling, no implicit repair, and preservation of existing exFAT data and rescue assets. Fail-closed discovery applies to the foundation boot profile only: it supersedes that profile's automatic SD fallback, while the legacy compatibility profile and its existing test contract remain unchanged and outside foundation acceptance.
- Verify actual generated startup/shutdown graphs and failure propagation, not merely script headers. Distinguish root-read-only/backing-flushed from root-loop-detached/outer-unmounted.
- Introduce venue-labelled host, image and PSTV acceptance records and a bounded operator runbook. Reconcile stale public status only during later implementation, with dated boundaries intact.

### Current verified state and evidence boundary

Inspection on 2026-09-17: outer branch `feat/persistent-root-boot`, HEAD `91378d570a06aa354482f72b460eb4e45f28b3f7`; kernel gitlink `37b9348710dfe1751dae0ef0fd2954b2714d08d4`. OpenSpec resolves to this repository; no pre-existing changes or capability specs were listed. Research HEAD is `8685d20e9434dd1c644d6a03e59c927e61748041`. No device was contacted during this planning session.

- **Verified by source inspection / host-verifiable:** `buildroot-vita/board/vita/overlay/init`, its `usr/share/vita-boot/select-root`, `debian/overlay/`, `debian/insserv-overrides/`, and the three boot/lifecycle tests already exist. `debian/README.md` explicitly says no image generator exists. Existing tests were inspected, not executed in this session.
- **Directly observed in archived output:** research `lab/usb-debian-2026-09-13/device-verify.log` contains the target image SHA-256; `boot-debian.log` records exact deployed zImage readback identity and two loader-to-SSH cycles. These logs alone do not establish PID 1 or package persistence.
- **Recorded hardware result, corroborated in part:** that directory's `RESULTS.md` §§6–7 records Debian PID 1, ext4 loop root over USB exFAT, override-to-rescue, `opened`/`confirmed`/`shutdown-synced` records, and a marker surviving a graceful restart through VitaOS and loader relaunch. It also reports native apt install/run and toolchain execution. Preserve those historical results without presenting them as a fresh test of current HEAD or complete package-lifecycle/reboot proof.
- **Known contradictions in the historical record, to be reconciled rather than smoothed over (task 1.1 owns this):** research `lab/usb-debian-2026-09-13/root-config.log:91–92` records `FAIL: vita-root-sync absent from stop graph`, which contradicts that directory's `RESULTS.md` shutdown-order claim; no committed runtime shutdown transcript, `findmnt`, `/proc/1/exe` or `uname` output exists for the trial; the suite file count differs between `RESULTS.md` (22,878) and `device-verify.log`/`root-config.log` (22,872); and `transfer-health.txt:351–352` records exFAT not-properly-unmounted warnings while `RESULTS.md` reports zero `unclean-shutdown` records. Consequently **no artifact in this change treats shutdown ordering as proven**, and the native apt transaction on PSTV is source-supported prose rather than a captured log — the committed apt evidence is a `qemu-arm-static` chroot, not the console. The research README's RAM-only rootfs wording is likewise stale relative to the USB trial.
- **Candidate / hardware-gated:** freshly rebuilt artifacts, install/upgrade/remove plus package database/config/program persistence, actual shutdown failure interlock, absent/invalid-root recovery matrix and repeatability. Full root-loop detach, backing-volume unmount and power-loss durability remain unproven.
- **Historical baseline to preserve:** Bluetooth HID was fully validated on PSTV (`vita-linux-research/README.md:36–38`); absence of a connected controller or a candidate gap is not evidence it never worked. Current-candidate regression acceptance remains separate.

### Non-goals and why this slice

No kernel/loader feature development or pin change; no repartitioning, native ext4 partition migration, automatic repair, live-root resizing, SD2Vita repair, handheld deployment, systemd/cgroup migration, Alpine implementation, full desktop/application tiers, DRM/SGX, audio, battery, suspend, direct power-on Linux, public image release, or destructive power-cut testing. Full unmount/power-loss-safe shutdown is not claimed by this bounded flush-based prototype.

The USB route avoids making SDIF1 diagnosis a prerequisite while preserving that independent lane. Debian/bookworm, sysvinit and 8 GiB follow the later trial and handoff, superseding the earlier trixie/16 GiB planning assumptions. This is smaller than the A0–H roadmap but closes the reproducibility and safety gaps around the experiment that already exists. Native Linux partitions remain the recorded long-term destination, and Alpine remains a welcome second target—not a rejected fallback.

## Capabilities

### New Capabilities

- `foundation-host-workflow`: bounded, isolated, non-deploying host build/test workflow.
- `foundation-artifact-contract`: locked inputs, inspectable rescue/root image pair, manifests and private enrollment.
- `pstv-root-boot`: explicit PSTV root selection, handoff and truthful confirmation.
- `persistent-package-state`: media identity, exFAT preservation and native package-state persistence.
- `pstv-root-lifecycle`: ordered graceful shutdown/restart and conservative failure handling.
- `foundation-hardware-validation`: exact-candidate PSTV acceptance and explicit per-model deferrals.
- `foundation-recovery-safety`: independent rescue, bounded retries, authorization and rollback.
- `foundation-evidence`: dated, venue-specific, reproducible positive/negative evidence.

### Modified Capabilities

None: the durable OpenSpec capability inventory is empty. These are first normative contracts, not claims that all underlying implementation is new.

## Impact

Implementation belongs to **vita-linux-next**, the outer integration project: `debian/`, existing Buildroot overlay boot scripts, `tests/`, `tools/`, `Makefile`, and integration documentation/evidence. Kernel, loader and Buildroot revisions remain explicit inputs with distinct artifact identities. Existing manual unpinned/implicit selection workflows will not qualify for foundation acceptance; no migration or overwrite happens automatically.

**vita-linux-research** is the read-only evidence/reverse-engineering lane for this change, never its OpenSpec home. Historical citations resolve there; new integration gate outputs are planned under `vita-linux-next/lab/pstv-foundation/<run-id>/`. No research files are edited.

Risks: nested ext4 cannot journal the outer exFAT; SysV failure exits alone do not prove reset prevention; mutable roots cannot retain their initial whole-image hash; a newly built image may lose firmware/credentials or regress proven input/network paths. Mitigations are explicit enrollment, immutable transfer identities, generated-graph tests, failure injection, offline filesystem checks, independent rescue and operator-owned hardware gates. Missing evidence blocks the affected claim; negative results remain valid deliverables, not successful milestone closure.

This request creates planning artifacts only under `openspec/changes/pstv-general-purpose-linux-foundation/`. It does not authorize implementation, source edits, device operations, commits, pushes, PRs, branch changes or remote changes.
