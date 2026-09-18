# Spec Delta

## Purpose

Select and boot only an explicitly enrolled PSTV persistent root, with truthful confirmation and an independently usable rescue path.

## ADDED Requirements

### Requirement: Fail-closed root admission

The foundation boot profile SHALL require an explicit PSTV enrollment binding USB transport, outer filesystem identity, root-image path, root instance/filesystem identity, architecture and intended init. It SHALL reject absent, ambiguous, cloned, malformed or mismatched candidates and internal MMC, without automatic SD fallback, formatting or repair. Read-only admission constrains the inner root image only (a non-mutating check, no automatic repair); it does not make the enrolled outer volume read-only, which remains writable because the durable attempt record required by the confirmation requirement must be committed before handoff. The legacy compatibility profile — including its existing automatic SD-fallback behavior and test contract — is retained unchanged and is outside foundation acceptance. Discovery SHALL be bounded and independent of device numbering.

#### Scenario: Numbering changes — host-verifiable H1 and PSTV gate P1
- **WHEN** exactly one enrolled USB root is present under a different device node
- **THEN** it is identified by its enrolled transport/filesystem/content contract rather than the old node name

#### Scenario: Enrolled USB is absent or ambiguous — host-verifiable H1 and PSTV gate P1
- **WHEN** the enrolled USB is absent, two matching copies are attached, or only an unrelated SD/FAT volume is available
- **THEN** boot selects recognizable rescue within the declared discovery timeout and performs no fallback root execution or write mount of unrelated media

#### Scenario: Invalid root or missing checker — host-verifiable H1 and image/build gate I1
- **WHEN** root identity/init/ABI is invalid, the required read-only filesystem check cannot run, or it reports a non-clean or indeterminate result
- **THEN** boot refuses normal-root admission, reports a bounded reason and does not automatically repair the image

### Requirement: Single checked handoff owner

Normal-root selection SHALL occur before rescue services or background storage workers start. The handoff SHALL preserve the root's backing mount and required device/runtime access, allocate only available resources, and unwind only resources it owns on pre-handoff failure. Cleanup failures SHALL be visible and inhibit competing automatic storage activity.

#### Scenario: Required mount move fails — host-verifiable H1 and image/build gate I2
- **WHEN** moving a required mount into the new root fails
- **THEN** no normal-root exec occurs, successful earlier moves are unwound where possible, and any unresolved ownership is reported in degraded rescue

#### Scenario: Loop device already belongs to another user — host-verifiable H1
- **WHEN** a preferred loop number is occupied
- **THEN** admission uses a verified free resource or refuses without detaching or modifying the occupied loop

#### Scenario: Native root starts — PSTV gate P1
- **WHEN** the approved exact candidate passes admission and handoff
- **THEN** the intended init runs as PID 1 on the enrolled rw ext4 root, the backing volume remains correctly attached, and local console plus authorized SSH are exercised

### Requirement: Durable and correlated boot confirmation

Before normal-root handoff, the system SHALL durably record the candidate and attempt identity. Confirmation SHALL match that attempt and be published only after intended PID 1, rw root/backing media, SSH listener and non-loopback IPv4/default-route readiness are verified. Write, sync, parse or identity failures SHALL not produce usable confirmation. Clock changes SHALL NOT reset retry accounting.

#### Scenario: Attempt record cannot be persisted — host-verifiable H1
- **WHEN** an attempt append/commit or its sync fails
- **THEN** normal-root handoff does not occur and rescue reports the durability failure

#### Scenario: Partial or stale confirmation — host-verifiable H1
- **WHEN** confirmation is partially written, cannot be synced, or names a different candidate/attempt
- **THEN** the next selection treats history as unconfirmed or unknown and does not reset the failure streak

#### Scenario: Usable boot and independent access — PSTV gate P1
- **WHEN** all readiness conditions pass for the current attempt
- **THEN** its durable confirmation is recorded and the hardware report separately captures PID 1, mounts, actual authenticated access and boot identity rather than treating a listening port as sufficient proof
