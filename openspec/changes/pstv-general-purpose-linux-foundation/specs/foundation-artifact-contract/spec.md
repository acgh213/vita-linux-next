# Spec Delta

## Purpose

Identify and inspect the separate rescue, kernel, loader and package-managed root artifacts needed for a repeatable PSTV trial without exposing private enrollment.

## ADDED Requirements

### Requirement: Complete paired artifact provenance

A foundation candidate SHALL have a machine-readable manifest identifying outer source, kernel/loader/Buildroot pins, resolved configurations, toolchain, model-specific DTB, embedded rescue initramfs, Debian suite/architecture/init, signed package inputs, installed versions, image size and artifact hashes. The manifest SHALL distinguish source identity, build identity, private enrollment and target readback. Missing required identities SHALL block candidate authorization.

#### Scenario: Candidate inspection completes — image/build gate I1
- **WHEN** all candidate artifacts and locked inputs are available for offline inspection
- **THEN** the manifest binds each artifact to its role and reports an independent hash/size for the root image, zImage, DTB and loader assets

#### Scenario: Misleading filename or stale submodule — image/build gate I1
- **WHEN** bytes, source pin or embedded release differ from the declared identity even though the filename looks correct
- **THEN** verification fails and the artifact is not accepted for deployment or rollback

### Requirement: Inspect the packed boot and root layers

Image acceptance SHALL verify the kernel's embedded initramfs against the intended rescue source and inspect the packed root for ARM hard-float userspace, intended init, required filesystem/loop/checking tools, firmware, network/SSH prerequisites, executable services, generated dependency graph and ephemeral runtime directories. Service-start suppression used during assembly SHALL be absent from the finalized bootable root. Source-tree presence SHALL NOT substitute for packed-image inspection.

#### Scenario: Complete image passes inspection — image/build gates I1 and I2
- **WHEN** a packed candidate has the intended boot entrypoint, matching rescue contents, required tool/firmware capabilities and effective service configuration
- **THEN** it is eligible for the separate hardware preflight, not marked boot-proven

#### Scenario: Chroot works but boot prerequisite is missing — image/build gate I1
- **WHEN** binaries execute under host emulation but firmware, DHCP/Wi-Fi support, authorized access, registered init services or the filesystem checker is absent from the privately enrolled pair
- **THEN** the boot-readiness gate fails with the missing prerequisite

### Requirement: Separate public assembly and private enrollment

Public inputs and distributable outputs SHALL contain no Wi-Fi secrets, SSH private keys or user authorized-key lists. Private enrollment SHALL be explicit, preserve the approved rescue access path and provision persistent normal-root identity with checked owner/modes and effective key-only access policy. Logs SHALL record credential metadata/fingerprints rather than contents. Firmware redistribution SHALL require separate rights review.

#### Scenario: Public artifact inspection — image/build gate I1
- **WHEN** an artifact is classified public
- **THEN** its unpacked content is scanned and private enrollment material is absent; any restricted firmware is withheld unless separately approved

#### Scenario: Private trial enrollment — image/build gate I1 and hardware gate P1
- **WHEN** a private candidate is prepared for an approved PSTV trial
- **THEN** secret modes and effective SSH policy pass inspection, expected host fingerprints are recorded, and actual key authentication is checked separately on PSTV

### Requirement: Transfer identity is not mutable runtime identity

The candidate SHALL retain a pre-mutation whole-image transfer hash and a separate enrolled instance/filesystem identity. Runtime package changes SHALL NOT be rejected merely because the original whole-image hash no longer matches, and a replacement image SHALL require explicit enrollment.

#### Scenario: Package installation changes image bytes — PSTV gate P2
- **WHEN** an authenticated package transaction changes the enrolled writable root
- **THEN** the system retains its instance identity and package state without requiring the original transfer hash to match its changed bytes

#### Scenario: Partial transfer — image/build gate I1 and PSTV preflight P0
- **WHEN** a transferred candidate's complete destination hash has not finished or differs from its source
- **THEN** activation is refused regardless of successful copy exit status or matching file size
