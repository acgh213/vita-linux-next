# SDIF1 / game-card (SD2Vita) bring-up plan and outcome

> **Historical plan — outcome recorded 2026-09-12.** This document began as a
> read-only-first bring-up plan. The PSTV result is now hardware-proven: SDIF1
> enumerates the SD2Vita automatically, the 128 GB exFAT card mounts cleanly,
> and the card hosts both the hash-verified toolkit payload and a persistent
> journalled ext4 workspace image. Read the outcome below before treating any
> unchecked plan item as current work.
>
> The remaining work is deliberately narrow: issue #16 separates the combined
> rail/rescan lever, issue #17 measures the generous 800 ms settle delay, and
> issue #19 explains the observed 5.297–155.633 s enumeration spread. The
> bounded background mount waiter is the shipped mitigation; it avoids making
> login or SSH wait for a late card.

## Outcome — hardware-proven 2026-09-12

- **Rail power was the blocker.** Syscon command `0x888` is firmware-proven;
  the earlier boot path left the game-card rail off. `sdhci_vita_probe()` now
  requests the rail before `sdhci_add_host()` and defers until the syscon is
  ready, so first MMC initialization is not issued against an unpowered slot.
- **The ordering fix worked.** The card registered and mounted read-only with
  zero command timeouts. The original 10 s failure timestamp was an init command
  issued before the rail came up, not evidence that a longer hardware timeout was
  needed.
- **Host numbering changed as a consequence of the defer.** SDIF2/Wi-Fi is now
  `mmc1`; SDIF1/game-card is `mmcblk2`. Consumers must identify the card by its
  filesystem and skip internal eMMC, never hard-code an `mmcblkN` name.
- **Persistence is real but deliberately isolated.** The writable store is a
  journalled ext4 *file* on the exFAT card, not a card repartition. Its POSIX
  semantics and journal recovery were tested without risking the card. See issue
  #20 for the complete record.
- **Do not overclaim the follow-on toolchain work.** A Buildroot image containing
  e2fsprogs, make, git, python3 and opkg packed successfully, but its full kernel
  image has not booted. Those tools are built artifacts, not yet a hardware-proven
  deployed capability. The current known-good running system remains the smaller
  toolkit-plus-workspace image.

**As of 2026-09-12.** Original planning baseline: `vita-linux-next` @
`0d1ba53a4376` (kernel), `acgh213/vita-linux-next` @ `ce7e3cd` (project).

**Device: PSTV first** (confirmed). The handheld Vita is a separate,
separately-characterized pass — PSTV USB results already do not transfer to it.

Goal: make a card in the game-card slot (`SDIF1`) enumerate and read under Linux
on PSTV, **read-only first**, with a recorded gate at every step.

This plan is deliberately front-loaded with a prerequisite that is *not* Linux
work, because skipping it would make every later result uninterpretable.

> **Provenance note.** Paths beginning `upstream/vita-linux-port/`, `docs/02-*`,
> `docs/03-*`, and `lab/**` refer to a separate local research tree
> (`vita-linux-research`) that is **not part of this repository** and is not
> published. Those citations are provenance pointers — the evidence behind a
> claim — not files a fresh clone can open. Everything else resolves in-repo.

> **Revision note (v2, 2026-09-12).** v1 of this plan gated on card-detect bit 16
> asserting. That was wrong — see "Card detection is already bypassed" below. The
> pivotal signal is now whether the card *responds to initialization*. Bit 16 is
> demoted to a secondary electrical indicator.

---

## Why this looks tractable

Four facts, all from our own source or records:

1. **The SDHCI driver already works.** `SDIF0` (internal eMMC) and `SDIF2` (SDIO
   Wi-Fi) both run on `sdhci-vita` today. The controller driver is not the
   unknown.
2. **The driver already contains the hooks for this.** `sdhci_vita_reinit_host()`
   and `sdhci_vita_trigger_rescan()` exist and are `EXPORT_SYMBOL_GPL`'d
   (`drivers/mmc/host/sdhci-vita.c:130`, `:367`). Their documented purpose is a
   full HW+SW reinit of an SDIF host **after a card power change**. That is
   exactly this problem.
3. **Protocol negotiation is core MMC's job.** Per the StorageMgr reverse
   engineering recorded in `upstream/vita-linux-port/PROGRESS.md`: no special
   register writes are needed to move SDIF1 from game-card mode to SD mode — the
   controller auto-negotiates from card responses (CMD0/CMD8/ACMD41 for SD vs
   CMD1 for MMC). VitaOS blocked SD-type cards on device index 1 *in software*
   (`SceSdstor`), and StorageMgr patched those checks. Linux's MMC core has no
   such block.
4. **The slot power control is known and has firmware provenance.** Syscon
   command `0x888` is `ksceSysconCtrlSdPower`, recovered from `syscon.skprx.elf`
   disassembly (`lab/battery-re/POWER-SYSCON-TELEMETRY-CHECKPOINT-2026-08-30.md`).
   This is **not** a guessed secure SMC, so it satisfies the standing rule.

---

## Card detection is already bypassed — this changes the gates

`sdhci-vita` sets `SDHCI_QUIRK_BROKEN_CARD_DETECTION` **unconditionally** for
every SDIF host (`drivers/mmc/host/sdhci-vita.c:496`). In the SDHCI core that
quirk means:

- `sdhci_get_cd()` returns **1 — "card is always present"** — and never reads
  `SDHCI_PRESENT_STATE` bit 16 (`drivers/mmc/host/sdhci.c`, `sdhci_get_cd()`).
- The core sets `MMC_CAP_NEEDS_POLL` (`sdhci.c:4491-4494`), so it **polls**.

Three consequences:

1. **The MMC core does not gate initialization on bit 16 -- but the Vita driver
   does.** The first revision of this plan collapsed those two layers into one
   and was wrong; corrected 2026-09-12 after reading `sdhci_vita_reinit_host()`
   properly:
   - In the *core*, the quirk means init is attempted on every SDIF bus
     regardless of the pin, so "the card-detect pin never asserts" is not what
     stops the *core*.
   - In the *driver*, `sdhci_vita_reinit_host()` reads `SDHCI_PRESENT_STATE`
     and wraps its entire power / voltage-select / clock-enable sequence in
     `if (val & BIT(16))` (`sdhci-vita.c:296` at time of writing). When that
     bit is clear, reinit returns having powered nothing and clocked nothing.
   So bit 16 **can** block SDIF1 init -- one layer below where this plan first
   looked, through our own reinit path rather than the core detect path. That
   makes it a first-class measurement, not a secondary indicator: it separates
   "the rail never came up" from "the rail is up and the card is silent".
2. **The real blocker is that initialization fails** — which is exactly what an
   unpowered card produces: no response to CMD0/CMD8/ACMD41.
3. **"Polling creates log spam" now has a precise mechanism.** `NEEDS_POLL` plus a
   card that never responds means every SDIF host polls and fails continuously.
   That is why SDIF1 and SDIF3 were disabled
   (`upstream/vita-linux-port/PROGRESS.md:282`).

### What the device tree therefore needs

**`status = "okay"` and nothing else.** Not `non-removable`, not `broken-cd`, not
a pwrseq or regulator reference — the quirk already supplies "always present" and
polling. This also explains the earlier commit
`24792fa542ad arm: vita: dts: remove non-removable from sdif1` (Cameron Clough,
Feb 2026): removing it changed only *which error string* appears, not whether
init is attempted.

The probe-time `present` read in the driver (`sdhci-vita.c:455`) is **diagnostic
logging only** — it prints `[card present]` / `[no card]` and gates nothing.

`sdif1` is currently disabled on **all three** boards (`vita1000.dts`,
`vita2000.dts`, `pstv.dts`). Nobody enables it today.

---

## The hypothesis to test

**Linux powers the game-card rail off and never turns it back on, so the card
never responds to initialization.**

Our own reboot notifier does this (`drivers/mfd/vita-syscon.c:727`):

```
vita_syscon_short_command_write(syscon, 0x89B, 0, 2);  /* MSIF */
vita_syscon_short_command_write(syscon, 0x888, 0, 2);  /* game card */
```

with the comment that peripherals are powered off "so VitaOS (on reboot) or Ernie
(on poweroff) finds controllers in a clean state."

Two consequences follow, and they are consistent with the observed failure:

- The polarity is established **by our own code**: `0x888` with data `0` is *off*.
- Nothing in the normal boot path ever writes `0x888` with data `1`. After the
  first Linux reboot, the game-card rail stays off for the whole session.

### Discriminators — decide these before running anything

| Observation after rail power-on + reinit + rescan | Interpretation | Next |
|---|---|---|
| Card initializes — CID/OCR read, SD command sequence progresses | rail power was the blocker; hypothesis confirmed | Gate B → read-only block validation |
| Bit 16 asserts, but init still fails | slot is powered, card still does not answer | adapter or media is not functional → Gate 0 becomes mandatory, not optional |
| Bit 16 never asserts **and** init fails | the `0x888` write had no effect or is the wrong lever, **or** the rail is up but no detect signal reaches the controller | re-verify the syscon command and the pervasive gate for `bus_index 1`; if the rail is confirmed up, bypass the `BIT(16)` gate in `sdhci_vita_reinit_host()` for bus 1 -- SD2Vita has no real detect switch and the gate is what leaves the card unpowered |
| Init fails specifically on CRC errors or command timeouts | signal integrity, or a speed/voltage issue | constrain to 3.3 V and a lower clock before touching the rail again |

Fixing this table in advance is the point. Otherwise a failing run teaches nothing.

---

## Non-goals

- **No writes.** No `mkfs`, no `fsck`, no formatting, no partition edits, on any
  medium. Reads only until Gate C explicitly passes.
- **Not the Sony memory card.** The proprietary memory card (MSIF, syscon `0x89B`)
  is a separate lane with authentication and crypto work, rated very-high
  difficulty. It is not this plan.
- **Not upstreaming.** This is lab work first.
- **Not the Vita handheld.** PSTV first, because it is the reliably deployable
  device with SSH and a framebuffer console.

---

## Existing baseline (verified 2026-09-12)

| host | reg | IRQ | role | in `pstv.dts` |
|---|---|---|---|---|
| `sdif0` | `0xE0B00000` | `GIC_SPI 188` | internal eMMC (M4G1FA, 3.55 GiB) | **okay** |
| `sdif1` | `0xE0C00000` | `GIC_SPI 189` | **game card / SD2Vita** | **not enabled** |
| `sdif2` | `0xE0C10000` | `GIC_SPI 190` | SDIO Wi-Fi SD8787 (`mmc-pwrseq = <&wlan_pwrseq>`) | **okay** |
| `sdif3` | `0xE0C20000` | `GIC_SPI 191` | microSD | not enabled |

Boot log instantiates exactly two hosts, matching the table. There is **no SDIF1
line at all** — no host exists, so nothing can detect a card.

`sdif2`'s Wi-Fi role is provable (its `mmc-pwrseq` is `&wlan_pwrseq`, and it
enumerates as the SD8787 that `mwifiex_sdio` binds). `sdif1`'s game-card role
comes from project records (`upstream/vita-linux-port/PROGRESS.md:70`;
`docs/history/HARDWARE-2026-02-inherited.md:35`), not a DT comment. This
repository's current `HARDWARE.md:31` carries the same claim as an
evidence-ranked row.

**Prior attempt:** `lab/bringup-2026-08-17/experimental/gamecard-sdif1-syscon-hooks.patch`
(DTS enable of `sdif1`+`sdif3`, plus `gamecard_power` and `syscon_cmd` sysfs
hooks). It was written and **parked without a recorded hardware evaluation**.
That is the main loose end this plan picks up.

---

## Acceptance gates

- **Gate 0 — outside Linux (prerequisite).** The same adapter and microSD are
  verified working in VitaOS on the same device.
- **Gate A.** After rail power-on and reinit, SDIF1 **initializes a card**: the
  SD command sequence progresses and CID/OCR are read.
- **Gate B.** Initialization completes to a ready state with a stable capacity.
- **Gate C.** Read-only block reads are stable — partition table readable, repeat
  reads match, no CRC or error-interrupt storms. Still zero writes.
- **Gate D.** Production posture decided: shipped on by default, opt-in, or off;
  with the polling cost measured and recorded.

A desktop compile is not a hardware pass. Nothing above is claimed until it has a
recorded gate from the device.

---

## Phase 0 — Preconditions

### Task 0.1: Prove the adapter and card work in VitaOS

This is the gate that makes everything else meaningful. Without it, "init fails"
cannot be attributed: an unpowered card and a dead adapter produce the same
result.

- [ ] Install a storage plugin (e.g. YAMT) on the PSTV.
- [ ] Confirm the adapter + microSD mount in VitaOS and that content persists.
- [ ] If it does **not** work in VitaOS, stop and fix that first.

**Citation care.** The "never verified working on this unit" statement comes from
`upstream/vita-linux-port/PROGRESS.md` and originally from an upstream
contributor's commit message (`24792fa542ad`, Feb 2026) describing **their** test
unit. It is evidence that this was never established upstream — not proof about
this PSTV. Gate 0 is what establishes it here.

### Task 0.2: Declare the test media and the rollback

- [ ] Use a **disposable** microSD. Never the only copy of anything.
- [ ] Record the deployed known-good kernel for rollback
      (`6.12.0-g0d1ba53a4376` is the current known-good).
- [ ] Confirm the loader can restore the previous image without the SD card.

### Task 0.3: Record device metadata

- [ ] PSTV model, firmware version, adapter model, card capacity and class.
- [ ] Whether the same adapter has ever worked on any device.

---

## Phase 1 — Audit the parked work (host only, no hardware)

### Task 1.1: Confirm the driver prerequisites

**Resolved during planning.** Recorded so nobody re-checks:

- `sdhci_vita_reinit_host(int bus_index)` — `sdhci-vita.c:130`, exported at `:358`
- `sdhci_vita_trigger_rescan(int bus_index)` — `sdhci-vita.c:367`, exported at `:377`
- the reinit path already waits for `CARD_STATE_STABLE` (bit 17) and then up to
  50 ms for bit 16 — `sdhci-vita.c:240-256`

### Task 1.2: Re-verify the `0x888` provenance and polarity

- [ ] Cite the firmware provenance (`ksceSysconCtrlSdPower`) in the commit body.
- [ ] Cite our own reboot handler as the polarity evidence.
- [ ] Confirm `cmd_len = 2` is consistent with the existing reboot usage.

### Task 1.3: Determine what the DT node needs

**Resolved during planning.** `status = "okay"` only — no `non-removable`, no
`broken-cd`, no pwrseq, no regulator, because `SDHCI_QUIRK_BROKEN_CARD_DETECTION`
already forces "always present" and enables polling. See "Card detection is
already bypassed".

- [ ] Confirm on hardware that a plain enable is sufficient; revisit only if it
      is not.

### Task 1.4: Decide the polling-spam mitigation

Polling an empty controller is why this was disabled, and the mechanism is now
known (`MMC_CAP_NEEDS_POLL` + a card that never answers). Options: accept it for
the lab lane only; gate the host behind rail power; ship read-only and off by
default with an opt-in overlay. Pick one and write it down.

### Task 1.5: Make the intent testable without hardware

- [ ] Add a build-contract check that the PSTV DTB's SDIF1 state matches the
      documented intent, so the DT cannot silently drift from this plan.

---

## Phase 2 — Controlled negative, then power-on (Gate A)

**Order matters.** The rail cannot be exercised before the host exists: with
`sdif1` disabled there is no host to reinit or rescan, and no register to read.
The DT enable must come first.

### Task 2.1: Reproduce the known negative deliberately

- [ ] Enable only `&sdif1 { status = "okay"; }` on `pstv.dts`.
- [ ] Boot and confirm: host registers, IRQ present, **init attempted and fails**,
      bit 16 not asserted.
- [ ] Capture the full boot log, the raw `SDHCI_PRESENT_STATE` value, and the
      exact init failure and error-interrupt status.
      The SDIF probe line already carries the pre-power-on baseline --
      `SDIF1 ... present 0x........` with a `[card present]` / `[no card]`
      decode -- so the "before" reading costs nothing to obtain.
- [ ] `cat` the slot attribute for the same register plus the decoded bit at
      any later point, including after the rail is powered on.

Recording this negative is what makes the next step interpretable.

### Task 2.2: Add a bounded, non-default power control

- [ ] Provide a way to write syscon `0x888` with data `1`, and `0` to restore.
      Prefer an explicit, reversible hook over anything that runs at boot.
- [ ] It must **not** be enabled by default in a shippable configuration.
- [ ] Bound it: no unbounded polling, no writes to any block device.

### Task 2.3: Power on, reinit, rescan, observe

- [ ] Write `0x888 = 1` **first**, then reinit. Order is load-bearing:
      `sdhci_vita_reinit_host()` only powers the card and enables its clock
      when bit 16 is set, so the rail has to be up before reinit runs or reinit
      is a no-op for power and clock.
- [ ] Reinit and rescan `bus_index 1`, then read the slot attr for the
      PRESENT_STATE value and the decoded bit.
- [ ] Record whether the card **initializes** (the pivotal signal), plus bit 16 as
      a secondary electrical indicator.
- [ ] Apply the discriminator table above **before** drawing a conclusion.

### Task 2.4: Gate A record

- [ ] Write the result up with raw register values, not a summary.

---

## Phase 3 — Card initialization (Gate B)

Only if Gate A shows the card responding.

- [ ] Trace OCR and CID read, then voltage negotiation, to a ready state.
- [ ] Capture where the sequence stops if it fails: CRC error, command timeout,
      or error-interrupt status.
- [ ] If CRC or timeouts appear, retry constrained to 3.3 V and a lower clock
      before blaming the rail.

---

## Phase 4 — Read-only block validation (Gate C)

- [ ] Read the partition table.
- [ ] Read blocks and repeat, comparing results.
- [ ] Measure error rate over a sustained read.
- [ ] Mount **read-only** only after reads are stable.

**No writes in this phase.** Writes are a separate decision with a separate gate,
after the read path is trustworthy.

---

## Phase 5 — Production posture (Gate D)

- [ ] Decide shipped-on / opt-in / off, with the polling cost measured.
- [ ] Update `docs/PROJECT-STATUS.md`, `HARDWARE.md`, and the roadmap so the
      documented state matches what was actually proven.
- [ ] Reconcile with the persistence design: an SD card is a candidate home for
      the writable workspace, and that interacts with the filesystem limits
      already documented in `docs/WORKBENCH.md`.

---

## Safety boundaries

- Read-only first. Never write, format, repartition, or repair from Linux.
- Disposable media only. Never a card holding the only copy of anything.
- Never touch the eMMC or SCE partitions in this lane.
- No guessed secure-world commands. `0x888` has firmware provenance; anything
  without it does not get written.
- One variable at a time. Each phase changes one thing and records one gate.
- Keep a known-good rollback deployed at all times.

---

## References

In this repository:

- `docs/PROJECT-STATUS.md` — current proven state
- `HARDWARE.md` — evidence-ranked hardware status, including the SD2Vita row
- `PROGRESS.md` — progress snapshot
- `docs/HARDWARE-ROADMAP.md` — sequencing, "SD2Vita and Vita memory card"
- `docs/history/HARDWARE-2026-02-inherited.md` — inherited hardware table, SDIF1 row
- `docs/WORKBENCH.md` — filesystem limits that constrain any persistence plan
- issue #10 — SD2Vita read-only characterization

Kernel source (in the pinned submodule):

- `drivers/mmc/host/sdhci-vita.c` — `:496` the card-detect quirk, `:130`/`:367`
  the reinit/rescan hooks, `:455` the diagnostic present read
- `drivers/mmc/host/sdhci.c` — `sdhci_get_cd()`, and `:4491-4494` where
  `MMC_CAP_NEEDS_POLL` is set
- `drivers/mfd/vita-syscon.c:727` — the reboot notifier that powers the game-card
  rail off

In the separate local research tree (provenance, not followable from a clone):

- `upstream/vita-linux-port/PROGRESS.md` — SD2Vita/SDIF1 section, StorageMgr RE
  findings, the `non-removable` failure, the log-noise rationale
- `docs/03-contribution-priorities.md` — existing Priority 2A for this work
- `docs/02-hardware-and-driver-gap-matrix.md` — SDIF1 row
- `lab/bringup-2026-08-17/experimental/gamecard-sdif1-syscon-hooks.patch` — parked attempt
- `lab/battery-re/POWER-SYSCON-TELEMETRY-CHECKPOINT-2026-08-30.md` — `0x888` provenance
