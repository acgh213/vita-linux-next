# Spec Delta

## Purpose

Keep a usable recovery boundary independent of the mutable root, with explicit operator authorization and no silent escalation into destructive storage or deployment work.

## ADDED Requirements

### Requirement: Rescue is independent and non-mutating by default

The foundation rescue environment SHALL boot without the Debian image, its credentials or enrolled USB medium, and SHALL provide a recognizable local recovery channel plus the separately provisioned rescue access path. Explicit rescue selection SHALL bypass candidate execution, filesystem repair and automatic write mounts, including legacy background workbench discovery. Boot overrides SHALL be editable with normal Linux stopped through the documented external-host method.

#### Scenario: Debian is broken but rescue assets are intact — PSTV gate P1
- **WHEN** the operator selects rescue on the offline removable medium and relaunches the approved loader/kernel
- **THEN** the initramfs remains the root, no Debian executable is run, automatic storage writes are suppressed, and rescue access is demonstrated independently

#### Scenario: USB removed while safely powered off — PSTV gate P1
- **WHEN** the enrolled root medium is absent at boot and other storage remains present
- **THEN** rescue becomes usable without choosing or write-mounting another root; no live-root hot-unplug is part of this test

### Requirement: Bounded next-boot retry policy

The selector SHALL choose rescue after three consecutive unconfirmed attempts; a valid intervening confirmation SHALL reset that streak. Unknown/corrupt history or incomplete shutdown SHALL fail closed. It SHALL distinguish absent override from malformed/unreadable override, support CRLF text, and offer an explicit reset path without silently discarding old evidence. A forced-normal override SHALL be one-shot and SHALL NOT bypass identity, health or durable-record gates.

#### Scenario: Repeated non-confirmation — host-verifiable H1 and PSTV gate P1
- **WHEN** three correlated attempts fail to reach usable confirmation
- **THEN** the next loader boot selects rescue and reports the retry-limit reason rather than attempting normal root forever

#### Scenario: Alternating success and failure — host-verifiable H1
- **WHEN** failed attempts are separated by valid confirmations
- **THEN** only the consecutive unconfirmed streak counts toward the rescue threshold

#### Scenario: Corrupt legacy state reset — host-verifiable H1
- **WHEN** the operator explicitly resets unparseable or legacy history
- **THEN** previous evidence is preserved, valid new state is initialized, and normal boot still requires the complete safety checks

#### Scenario: Malformed override or unsafe forced attempt — host-verifiable H1
- **WHEN** an override is empty, unreadable or unknown, or forced-normal is requested with invalid root identity or failed durability
- **THEN** normal boot is refused; CRLF encoding alone is accepted for a valid override and a valid forced attempt is consumed once

### Requirement: Hardware operations require an authorization packet

No deployment, target write, reboot, poweroff, repair, format, resize or network mutation SHALL follow merely from planning completion or host passes. An assigned hardware owner SHALL obtain a supervised window and approval for exact artifacts, target/media identity, backup/restore evidence, commands, timeouts and rollback. Faults, hash mismatch, unexplained regressions or incomplete checks SHALL stop progression.

#### Scenario: Planning graph is complete — host-verifiable process gate
- **WHEN** OpenSpec reports all planning artifacts present
- **THEN** implementation remains a separate request and device operations remain blocked on their separate authorization packet

#### Scenario: Approval or backup is missing — PSTV preflight P0
- **WHEN** an operator window, target identity, restorable backup or exact rollback artifact is not established
- **THEN** no trial staging or reboot occurs and the missing prerequisite is reported

### Requirement: Rollback is demonstrated rather than named

The rollback plan SHALL preserve an independently verified known-good boot path and the original mutable root/data. It SHALL document recovery when normal Linux, rescue networking or the candidate kernel is unavailable, verify restored artifact readback and demonstrate the restored boot. A backup filename alone SHALL NOT establish rollback capability.

#### Scenario: Candidate fails — PSTV recovery gate
- **WHEN** an approved trial fails or cannot safely continue
- **THEN** the owner preserves the failure record, uses the documented override or VitaOS restoration route, verifies the restored bytes and confirms the known-good environment before ending the gate

#### Scenario: Claimed rollback artifact identity differs — image/build gate I1
- **WHEN** a saved image's size/hash/embedded release differs from its advertised identity
- **THEN** it is rejected until provenance is resolved and is not deployed merely because its filename says proven
