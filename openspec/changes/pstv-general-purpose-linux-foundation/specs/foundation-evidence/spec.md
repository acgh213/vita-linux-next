# Spec Delta

## Purpose

Keep planning, implementation and hardware claims traceable to dated source-specific evidence, preserving negative results and the integration/research repository boundary.

## ADDED Requirements

### Requirement: Evidence records are re-derivable and venue-labelled

Each acceptance record SHALL identify its gate, date/timezone, execution venue, exact source/artifact identities, commands, exit status, expected/observed output, timeout, device/model/boot ID when relevant, and recovery disposition. It SHALL include what the result does not prove and classify claims using verified, directly observed, corroborated, inferred, candidate, unknown, deferred, hardware-gated or host-verifiable as appropriate. Command text without completed output SHALL NOT be treated as execution evidence.

#### Scenario: Reusing the September 13 trial — host-verifiable evidence review
- **WHEN** a plan cites the historical USB Debian result
- **THEN** it names the research record and tested artifacts, distinguishes raw output from narrative corroboration, and does not promote them to a fresh current-HEAD pass

#### Scenario: Host chroot package test — image/build gate I1
- **WHEN** an ARM package lifecycle runs under host emulation
- **THEN** the record identifies the host/emulator venue and explicitly excludes target PID1, hardware boot and reboot persistence claims

### Requirement: Negative and incomplete results retain their meaning

The evidence workflow SHALL preserve negative results and classify skipped, timed-out, truncated or unobservable checks as not run, blocked or inconclusive rather than passed. Absence of failure messages SHALL NOT establish health without affirmative evidence that the relevant device/operation was present and exercised.

#### Scenario: Check times out without output — any venue
- **WHEN** a full-image hash or other gate terminates before producing its required result
- **THEN** that gate is inconclusive, regardless of earlier copy success or correct byte count

#### Scenario: Device never enumerated — PSTV gate
- **WHEN** no storage-removal events occur because the medium never enumerated
- **THEN** the result is absent/not exercised rather than stable media

#### Scenario: Failed acceptance — any venue
- **WHEN** a complete method produces a negative result
- **THEN** the result is retained with its conditions and next bounded question, but the positive milestone requirement remains incomplete

### Requirement: Preserve chronology and repository ownership

OpenSpec SHALL remain solely in the vita-linux-next integration repository. This change SHALL treat vita-linux-research as read-only historical evidence and record new integration outcomes in vita-linux-next. Status reconciliation SHALL cite the dated superseding record without erasing the older result or borrowing its authority for a different build.

#### Scenario: Later evidence contradicts a summary — host-verifiable review
- **WHEN** an early RAM-root summary conflicts with the later USB-root experiment
- **THEN** the integration claim index records the chronological distinction, preserves both citations and states the remaining package/lifecycle acceptance gaps

#### Scenario: Evidence path is not available — host-verifiable review
- **WHEN** a referenced output cannot be found or a script has no retained completion log
- **THEN** the claim is labelled as report-only/unknown as applicable and a fresh capture is required for acceptance instead of inventing the missing output

### Requirement: Published evidence excludes secrets

Evidence SHALL include only credential presence, ownership/modes and fingerprints or hashes where identity is needed, never credential contents. Private images and restricted firmware SHALL be classified explicitly and SHALL NOT be included in public evidence bundles by default.

#### Scenario: Record a private enrollment check — image/build gate I1
- **WHEN** the credential-bearing candidate is inspected
- **THEN** the report records content-free metadata, confirms that no key material was captured and excludes the private artifact from publishable outputs
