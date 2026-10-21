# Spec Delta

## Purpose

Define observable graceful lifecycle guarantees for the PSTV loop-backed root without overstating full unmount, device-power or crash-safety behavior.

## ADDED Requirements

### Requirement: Generated shutdown order protects nested storage

The candidate SHALL generate startup and shutdown dependency graphs from its actual installed packages and verify both poweroff and reboot paths. Shutdown SHALL stop writers, release owned optional mounts/non-root loops, make the root read-only while its backing volume remains writable, verify those states and flush the backing volume before terminal reset/poweroff. It SHALL NOT detach the active root loop or treat header text as proof of execution order.

#### Scenario: Both generated graphs are valid — image/build gate I2
- **WHEN** the locked root is assembled and again after the package upgrade gate
- **THEN** its actual startup, poweroff and reboot graphs show the required order, include the required services, contain no dependency cycles and place usable-boot confirmation after readiness services

#### Scenario: Root remains writable — host-verifiable H1 and image/build gate I2
- **WHEN** the root read-only transition fails or reports success without changing observed state
- **THEN** no successful backing-flushed completion is accepted and terminal shutdown authorization remains denied

### Requirement: Terminal actions are fail-closed

The cooperative poweroff/reboot path SHALL require affirmative completion of every owned shutdown stage, including final backing-store handling. Missing state, a failed or unverifiable unmount/detach/flush of an owned non-root resource (never the active root loop or its backing volume, neither of which this milestone claims to detach or unmount), or failed command enumeration SHALL inhibit automatic terminal reset/poweroff and produce an operator-visible fault. A nonzero intermediate script exit alone SHALL NOT satisfy this requirement. Forced kernel reset and physical power removal are outside this guarantee.

#### Scenario: Earlier script failure reaches the terminal boundary — image/build gate I2
- **WHEN** a failure is injected into each stage while the generated shutdown sequence is executed with terminal hardware commands replaced by fixtures
- **THEN** the terminal action is never called, the failing stage is named, and no successful completion verdict is produced

#### Scenario: Apparent detach success leaves a loop — host-verifiable H1
- **WHEN** an owned non-root detach command exits zero but the device remains attached, or enumeration itself fails
- **THEN** the actual-state verification fails and automatic shutdown does not proceed as though cleanup succeeded

#### Scenario: Guard survives package maintenance — image/build gate I2 and PSTV gate P2
- **WHEN** the relevant package lifecycle regenerates services or terminal hooks
- **THEN** ordering and terminal refusal tests still pass, or the candidate is blocked before another hardware lifecycle trial

### Requirement: Durable shutdown state has narrow meaning

The system SHALL persist a shutdown-pending state before stopping writers and associate lifecycle records with the current boot/instance. An incomplete or ambiguous shutdown SHALL force next-boot rescue. A backing-flushed result SHALL attest only that root was read-only and the writable backing volume was successfully flushed; it SHALL NOT assert root-loop detach, outer unmount or power-loss safety.

#### Scenario: Shutdown completes the declared stages — PSTV gate P3
- **WHEN** the approved candidate performs an orderly lifecycle transition
- **THEN** recorded phase ordering, actual mount states and terminal outcome establish the declared root-ro/backing-flushed result, with full detach/unmount explicitly unproven unless separately observed

#### Scenario: Failure cannot be written to media — host-verifiable H1 and image/build gate I2
- **WHEN** a later shutdown fault prevents a durable failure append
- **THEN** the prior pending state remains conservative for next boot, an alternate console fault is emitted and absence of an unclean-shutdown line is not accepted as success

### Requirement: Lifecycle acceptance includes independent post-state

PSTV lifecycle acceptance SHALL include distinct boot IDs, package/file persistence, observed orderly terminal transitions and offline read-only checks of both inner and outer filesystems before subsequent Linux writes. It SHALL cover at least two graceful restart transitions through VitaOS/loader and one orderly poweroff/cold start. A mounted exFAT dirty flag SHALL NOT be used as retrospective clean-shutdown proof.

#### Scenario: Complete lifecycle sample — PSTV gates P2 and P3
- **WHEN** the declared restart and cold-start samples finish with captured order, persistent state and clean offline checks
- **THEN** the candidate receives only the bounded graceful-lifecycle verdict for that exact artifact/package set

#### Scenario: Offline check is missing or non-clean — PSTV gate P3
- **WHEN** the medium cannot be examined before next-boot writes, or a filesystem check is non-clean, timed out or incomplete
- **THEN** the corresponding clean-lifecycle gate remains blocked/inconclusive and no automatic repair is performed
