# SDIF1 / game-card (SD2Vita) bring-up plan

**As of 2026-09-12.** Baseline: `vita-linux-next` @ `0d1ba53a4376` (kernel),
`acgh213/vita-linux-next` @ `ce7e3cd` (project).

Goal: make a card in the game-card slot (`SDIF1`) enumerate and read under Linux
on PSTV, **read-only first**, with a recorded gate at every step.

This plan is deliberately front-loaded with a prerequisite that is *not* Linux
work, because skipping it would make every later result uninterpretable.

> **Provenance note.** Paths beginning `upstream/vita-linux-port/`, `docs/02-*`,
> `docs/03-*`, and `lab/**` refer to a separate local research tree
> (`vita-linux-research`) that is **not part of this repository** and is not
> published. Those citations are provenance pointers — the evidence behind a
> claim — not files a fresh clone can open. Everything else resolves in-repo.

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
   register writes are needed
   to move SDIF1 from game-card mode to SD mode — the controller auto-negotiates
   from card responses (CMD0/CMD8/ACMD41 for SD vs CMD1 for MMC). VitaOS blocked
   SD-type cards on device index 1 *in software* (`SceSdstor`), and StorageMgr
   patched those checks. Linux's MMC core has no such block.
4. **The slot power control is known and has firmware provenance.** Syscon
   command `0x888` is `ksceSysconCtrlSdPower`, recovered from `syscon.skprx.elf`
   disassembly (`lab/battery-re/POWER-SYSCON-TELEMETRY-CHECKPOINT-2026-08-30.md`).
   This is **not** a guessed secure SMC, so it satisfies the standing rule.

---

## The hypothesis to test

**Linux powers the game-card rail off and never turns it back on.**

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

An unpowered slot cannot assert card detect, which matches the standing negative:
the controller registers and takes an IRQ, but `SDHCI_PRESENT_STATE` bit 16 never
asserts, and forcing `non-removable` yields *"Failed to initialize a non-removable
card"* (`upstream/vita-linux-port/PROGRESS.md`).

### Discriminators — decide these before running anything

| Observation after power-on + reinit | Interpretation | Next |
|---|---|---|
| bit 16 asserts | power-gated slot confirmed | proceed to card init |
| bit 16 asserts **without** power-on | slot detect is independent of the rail | re-examine the earlier negative; power is not the blocker |
| bit 16 never asserts, either way | detect is not the SDIF present bit | investigate Syscon insert status / GPIO, not SDHCI |
| bit 16 asserts, init then fails on CRC/timeouts | signal integrity or speed mode | retry at 3.3 V only / lower clock, do not chase the rail |

Writing this table down first is the point. Otherwise a failing run tells us
nothing.

---

## Non-goals

- **No writes.** No `mkfs`, no `fsck`, no formatting, no partition edits, on any
  medium. Reads only until Gate C explicitly passes.
- **Not the Sony memory card.** The proprietary memory card (MSIF, syscon
  `0x89B`) is a separate lane with authentication and crypto work, rated
  very-high difficulty. It is not this plan.
- **Not upstreaming.** This is lab work first.
- **Not the Vita handheld.** PSTV first, because it is the reliably deployable
  device with SSH and a framebuffer console. Handheld parity is a later,
  separately-characterized pass — PSTV USB results already do not transfer.

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

**Why it is off:** it was disabled on purpose — *"SDIF1 (game card) and SDIF3
(microSD) disabled to avoid polling log"* noise
(`upstream/vita-linux-port/PROGRESS.md:282`). It is not a regression, and
re-enabling it will bring that noise back. That has to be handled, not ignored.

**Prior attempt:** `lab/bringup-2026-08-17/experimental/gamecard-sdif1-syscon-hooks.patch`
(DTS enable of `sdif1`+`sdif3`, plus `gamecard_power` and `syscon_cmd` sysfs
hooks). It was written and **parked without a recorded hardware evaluation**.
That is the main loose end this plan picks up.

---

## Acceptance gates

- **Gate 0 — outside Linux (prerequisite).** The same adapter and microSD are
  verified working in VitaOS on the same device.
- **Gate A.** After rail power-on and reinit, `SDHCI_PRESENT_STATE` bit 16
  asserts on SDIF1.
- **Gate B.** Card initialization progresses: OCR and CID read, voltage
  negotiation, SD command sequence reaching a ready state.
- **Gate C.** Read-only block reads are stable — partition table readable, repeat
  reads match, no CRC or error-interrupt storms. Still zero writes.
- **Gate D.** Production posture decided: shipped on by default, opt-in, or
  off; with the log-noise cost measured and recorded.

A desktop compile is not a hardware pass. Nothing above is claimed until it has a
recorded gate from the device.

---

## Phase 0 — Preconditions

### Task 0.1: Prove the adapter and card work in VitaOS

This is the gate that makes everything else meaningful.
`upstream/vita-linux-port/PROGRESS.md` states the SD2Vita "has never been
verified working on this unit," and that no storage plugin is installed
(`ur0:tai/config.txt` has no storage plugin).

- [ ] Install a storage plugin (e.g. YAMT) on the target device.
- [ ] Confirm the adapter + microSD mount in VitaOS and that content persists.
- [ ] If it does **not** work in VitaOS, stop. Fix that first. A Linux failure
      under an unproven adapter cannot be diagnosed.

### Task 0.2: Declare the test media and the rollback

- [ ] Use a **disposable** microSD. Never the only copy of anything.
- [ ] Record the deployed known-good kernel for rollback
      (`6.12.0-g0d1ba53a4376` is the current known-good).
- [ ] Confirm the loader can restore the previous image without the SD card.

### Task 0.3: Record device metadata

- [ ] Model, firmware version, adapter model, card capacity and class.
- [ ] Whether the same adapter has ever worked on *any* device.

---

## Phase 1 — Audit the parked work (host only, no hardware)

### Task 1.1: Confirm the driver prerequisites

Done during planning; record it explicitly so nobody re-checks:

- `sdhci_vita_reinit_host(int bus_index)` — `sdhci-vita.c:130`, exported at `:358`
- `sdhci_vita_trigger_rescan(int bus_index)` — `sdhci-vita.c:367`, exported at `:377`
- the reinit path already waits for `CARD_STATE_STABLE` (bit 17) and then up to
  50 ms for card detect (bit 16) — `sdhci-vita.c:240-256`

### Task 1.2: Re-verify the `0x888` provenance and polarity

- [ ] Cite the firmware provenance (`ksceSysconCtrlSdPower`) in the commit body.
- [ ] Cite our own reboot handler as the polarity evidence.
- [ ] Confirm `cmd_len = 2` is correct and consistent with the reboot usage.

### Task 1.3: Determine what the DT node actually needs

Do not assume `status = "okay"` is sufficient.

- [ ] Decide between plain enable, `non-removable`, `broken-cd`, `no-1-8-v`, or
      a pwrseq/regulator reference.
- [ ] Note the history: `24792fa542ad arm: vita: dts: remove non-removable from sdif1`
      — `non-removable` was tried and removed. Understand why before re-adding it.
- [ ] Check whether the pervasive gate/reset for `bus_index 1` is handled by the
      existing `sdhci_vita_pervasive_init()` path (`sdhci-vita.c:58`) or needs
      explicit enablement.

### Task 1.4: Decide the log-noise mitigation

Polling an empty controller is why this was disabled. Options: accept it for the
lab lane only; gate detection behind rail power; ship read-only and off by
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
- [ ] Boot and confirm the standing failure: host registers, IRQ present, bit 16
      **not** asserted, no card.
- [ ] Capture the full boot log and the raw `SDHCI_PRESENT_STATE` value.

Recording this negative is what makes the next step interpretable.

### Task 2.2: Add a bounded, non-default power control

- [ ] Provide a way to write syscon `0x888` with data `1`, and to write `0` to
      restore. Prefer an explicit, reversible hook over anything that runs at boot.
- [ ] It must **not** be enabled by default in a shippable configuration.
- [ ] Bound it: no unbounded polling, no writes to any block device.

### Task 2.3: Power on, reinit, rescan, observe

- [ ] Write `0x888 = 1`.
- [ ] Reinit and rescan `bus_index 1`.
- [ ] Read `SDHCI_PRESENT_STATE` bit 16 and capture the log.
- [ ] Apply the discriminator table above **before** drawing a conclusion.

### Task 2.4: Gate A record

- [ ] Write the result up with the raw register values, not a summary.

---

## Phase 3 — Card initialization (Gate B)

Only if Gate A passes.

- [ ] Trace OCR and CID read, then voltage negotiation.
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

- [ ] Decide shipped-on / opt-in / off, with the log-noise cost measured.
- [ ] Update `docs/PROJECT-STATUS.md`, `HARDWARE.md`, and the gap matrix so the
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

In the separate local research tree (provenance, not followable from a clone):

- `upstream/vita-linux-port/PROGRESS.md` — SD2Vita/SDIF1 section, StorageMgr RE
  findings, the `non-removable` failure, the log-noise rationale
- `docs/03-contribution-priorities.md` — existing Priority 2A for this work
- `docs/02-hardware-and-driver-gap-matrix.md` — SDIF1 row
- `lab/bringup-2026-08-17/experimental/gamecard-sdif1-syscon-hooks.patch` — parked attempt
- `lab/battery-re/POWER-SYSCON-TELEMETRY-CHECKPOINT-2026-08-30.md` — `0x888` provenance
