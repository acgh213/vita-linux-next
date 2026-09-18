# Spec Delta

## Purpose

Preserve existing removable-media data while establishing that package-managed state belongs to a persistent PSTV root and survives the declared lifecycle gates.

## ADDED Requirements

### Requirement: Preserve the existing storage layout and data

The foundation SHALL add only explicitly approved, separately named trial artifacts to enrolled USB exFAT media after backup, restore-test, read-only health and space gates. It SHALL NOT repartition, format physical media, resize a mounted image, repair a filesystem automatically, overwrite existing roots/workspaces/toolkits or write internal VitaOS/eMMC storage. The first trial SHALL use an 8 GiB ext4 file with target capacity budgeted by apparent rather than compressed or sparse size.

#### Scenario: Safe candidate staging — PSTV preflight P0
- **WHEN** the operator has approved identified media and backup/restore, offline health and free-space evidence passes
- **THEN** the separately named candidate is staged and fully hash-verified before activation, with the original files and layout preserved

#### Scenario: Dirty media or insufficient capacity — host-verifiable H1 and PSTV preflight P0
- **WHEN** a filesystem check is non-clean/incomplete, a backup restore has not been demonstrated, or free space cannot accommodate the fully allocated candidate and retained rollback
- **THEN** staging is blocked without repair, deletion or repartitioning

### Requirement: Persistent and ephemeral state remain distinct

The normal root SHALL persist installed packages, package databases, configuration, home/project files and machine/SSH identity. Runtime state SHALL be recreated per boot. Optional legacy toolkit/workspace files SHALL remain unchanged and SHALL NOT be required for foundation boot; automatic attachment is disabled in the foundation profile.

#### Scenario: Restart retains selected state — PSTV gate P2
- **WHEN** a package, edited configuration, home marker and compiled program are recorded before a graceful restart through VitaOS and loader relaunch
- **THEN** the same installed package version/database entries, file contents, executable behavior and SSH fingerprint are verified after a distinct boot, while ephemeral runtime state is fresh

#### Scenario: Legacy workspace is unavailable — host-verifiable H1 and PSTV gate P1
- **WHEN** no legacy toolkit/workspace is present or the SD card does not enumerate
- **THEN** the enrolled USB root still boots without creating substitute persistent-looking directories in RAM or mutating old storage

### Requirement: Native authenticated package lifecycle

Foundation acceptance SHALL exercise authenticated repository metadata retrieval, package install/run, an actual version-changing upgrade, and removal on the PSTV normal root. It SHALL record repository/keyring/index and package version provenance, maintainer-script/service outcomes, and a clean package-database audit. A host chroot, executable file's presence or no-op upgrade SHALL NOT satisfy native acceptance.

#### Scenario: Complete package lifecycle — PSTV gate P2
- **WHEN** the approved signed input set supplies a pinned initial and newer package version
- **THEN** native install, execution, upgrade and removal complete with recorded version transitions, clean database audit, observed service outcomes and persistence checks across restart

#### Scenario: Upgrade does not change version — PSTV gate P2
- **WHEN** the selected package is already current or the intended newer authenticated input is unavailable
- **THEN** the upgrade gate is blocked/not exercised rather than passed on an exit-zero no-op

### Requirement: No claim of untested power-loss durability

Storage acceptance SHALL distinguish graceful persistence, filesystem structural health and unclean-power-loss recovery. The ext4 journal SHALL NOT be represented as protection for outer exFAT allocation metadata or device caches.

#### Scenario: Graceful cycle succeeds — PSTV gates P2 and P3
- **WHEN** marker/package checks and offline filesystem checks pass after the planned orderly cycles
- **THEN** the record states graceful persistence under those conditions and leaves sudden-power-loss, root hot-removal and native-partition migration explicitly deferred
