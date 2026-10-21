# Spec Delta

## Purpose

Make the PSTV foundation reproducible through explicit, isolated host workflows that cannot silently cross the hardware deployment boundary.

## ADDED Requirements

### Requirement: Explicit host inputs and refusal boundaries

The host workflow SHALL accept explicit source pins, target profile, input lock and fresh output location; SHALL report tool versions, writable scratch and required space; and SHALL refuse missing dependencies, wrong architecture, unverified inputs, unsafe output paths and existing output rather than overwrite or silently substitute defaults. Validation and dry-run SHALL NOT install host dependencies, mount real devices, contact a console or perform deployment.

#### Scenario: Fresh host preflight succeeds — host-verifiable H1
- **WHEN** a supported Linux host supplies all declared dependencies, locked inputs and sufficient writable scratch for a new PSTV output
- **THEN** preflight reports the resolved inputs, commands and space budget without changing the target or producing a hardware-pass claim

#### Scenario: Unsafe or incomplete input refuses — host-verifiable H1
- **WHEN** the output already exists, resolves through an unsafe symlink, is a raw block device, space is insufficient, or a required tool/input is unavailable
- **THEN** preflight fails with the specific reason and leaves existing data unchanged

### Requirement: Repeatable assembly from authenticated inputs

The workflow SHALL reproduce the locked package/version inventory and declared non-secret configuration in two fresh assemblies. It SHALL record host tools, cross-toolchain identity, source/configuration inputs and expected nondeterminism. It SHALL NOT claim byte-identical images merely because both assemblies succeed.

#### Scenario: Two builds use one lock — image/build gate I1
- **WHEN** the candidate is assembled twice in separate fresh output directories from the same verified lock and source pins
- **THEN** inventory and normalized configuration comparisons pass and differing UUIDs, timestamps or private identities are explicitly classified

#### Scenario: Locked input cannot be obtained — host-verifiable H1
- **WHEN** a locked package or signed index cannot be verified or retrieved
- **THEN** assembly stops without selecting a newer package, different suite or unauthenticated repository

### Requirement: Repository-owned gates remain non-deploying

Host regression gates SHALL be callable through repository-owned targets and distinguish nonprivileged fixtures from explicitly enabled privileged image tests. CI SHALL use the same gate entrypoints, exclude private enrollment and never imply a target boot from a host or DTB result.

#### Scenario: Ordinary regression run — host-verifiable H1
- **WHEN** the foundation host tests run locally or in untrusted-contribution CI
- **THEN** device-facing commands are fixtures, no real mounts/deployments occur, and all required fixture failures propagate as nonzero results

#### Scenario: Privileged image test not approved — image/build gate I2
- **WHEN** a workflow lacks explicit permission for disposable-image mount tests
- **THEN** those tests are reported not run and their acceptance remains open rather than being counted as passed
