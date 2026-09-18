# Spec Delta

## Purpose

Require exact-artifact, venue-specific hardware acceptance for the PSTV foundation while preserving historical successes and keeping handheld claims separate.

## ADDED Requirements

### Requirement: Hardware acceptance is bound to an exact candidate

A hardware gate SHALL record device model/firmware, loader identity, kernel configuration/release, DTB, embedded rescue, root-instance and pre-mutation artifact manifest, package state and boot ID. Its verdict SHALL apply only to those recorded conditions. Host, emulator and earlier candidate results SHALL NOT satisfy a new candidate's target gates.

#### Scenario: Fresh candidate has host passes only — image/build gate I1
- **WHEN** a rebuilt pair passes host and image inspection but has not run on PSTV
- **THEN** its hardware state remains hardware-gated/not run, regardless of the September 13 trial or identical kernel source pin

#### Scenario: Candidate changes between hardware gates — PSTV gates P1 through P4
- **WHEN** a boot artifact, enrollment contract or relevant lifecycle package changes after a gate
- **THEN** affected acceptance is invalidated for the new candidate and the record preserves the older pass under its original identity

### Requirement: Complete foundation hardware matrix

A successful foundation verdict SHALL require approved preflight, independent rescue, absent/invalid-root and retry recovery, normal PID1/local console/key-auth SSH, native package lifecycle/persistence, the declared graceful lifecycle samples, offline health checks and exercised rollback. Bounded SMP, framebuffer/local-console, connected USB keyboard, Wi-Fi/authorized-SSH and USB-root regressions SHALL be required core components. Bluetooth controller operation and pairing-state persistence SHALL be reported as a separate compatibility result that neither closes nor by itself blocks the core verdict, while an absent required keyboard blocks the input component rather than passing it. Missing equipment or a failed prerequisite SHALL remain a visible blocker; it SHALL NOT silently reduce the matrix.

#### Scenario: All planned PSTV gates finish — PSTV gates P0 through P4
- **WHEN** each required gate has a completed evidence record on the frozen candidate and rollback is demonstrated
- **THEN** the foundation can be marked hardware-accepted with its stated boundaries, not a consumer-ready or all-model distribution

#### Scenario: Observation window expires — PSTV gate P1
- **WHEN** SSH has not appeared within the declared bounded observation window
- **THEN** the gate records the missing channel, inspects HDMI/boot history and network confounds, and remains inconclusive unless evidence establishes a specific failure

### Requirement: Protect historically validated input and network paths

The candidate SHALL retain the recorded kernel/firmware baseline. Its **core** regression set is SMP, framebuffer/local console, connected USB keyboard input, Wi-Fi/authorized SSH and USB-root access. Its **separately reported** compatibility result is previously validated Bluetooth controller operation plus pairing-state persistence. Controller absence SHALL NOT be described as historical Bluetooth HID failure, and it SHALL NOT by itself close or block the core verdict.

#### Scenario: Connected-device regression — PSTV gate P4
- **WHEN** the required keyboard and previously validated controller are present during the supervised candidate gate
- **THEN** actual physical input, local console behavior, Bluetooth controller operation and network/storage access are recorded against the candidate, with pairing persistence checked across the declared restart

#### Scenario: No controller available — PSTV gate P4
- **WHEN** a required controller is disconnected or unavailable
- **THEN** the current-candidate controller gate is blocked/not exercised while the historical PSTV Bluetooth HID success remains intact

### Requirement: Device-specific and deferred capability boundaries

The foundation SHALL declare PSTV USB-root scope only. Vita 1000/2000 persistent-root, handheld USB, battery/charging, audio streaming, new DRM/SGX work, suspend and native-partition migration SHALL remain deferred. Their eventual support SHALL require separate model-specific hardware evidence rather than inheritance from PSTV.

#### Scenario: PSTV acceptance succeeds — host-verifiable documentation gate and deferred V1
- **WHEN** the PSTV foundation matrix passes
- **THEN** support records still show Vita-specific gates as deferred/not run and do not advertise handheld parity, audio or accelerated graphics

#### Scenario: Handheld work resumes later — deferred Vita hardware gate V1
- **WHEN** a later change proposes Vita deployment
- **THEN** its own model, firmware, loader, storage and recovery inventory and native boot/lifecycle gates are required before any support claim; this change supplies no passed V1 result
