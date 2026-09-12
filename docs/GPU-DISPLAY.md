# GPU and display paths

**Status:** 2026-09-12

## Direct answer

The September timer, PMU, and production-OHCI work does **not** open a new PowerVR SGX acceleration path.

- CP14/CP15 are CPU coprocessor interfaces. Secure-world gating of CP14 debug registers and successful CP15 PMU counting describe CPU debug/performance access, not SGX power, firmware, MMU, or command submission.
- The corrected timer makes Linux timing truthful and helps every timed subsystem, but it does not expose GPU registers or satisfy the SGX power-domain gate.
- Production USB expands usable input, storage, and external-audio options; it is not a GPU transport.

SGX work therefore remains on hold until there is new secure-world provenance or another safe, reversible lever. The standing rule is no more guessed secure SMC calls.

## What *is* newly viable: a standard display path

The project has a complete evidence-backed route to DRM/KMS display without waiting for SGX:

- IFTU mapping corrected;
- mode enumeration works;
- vblank IRQ is real;
- inactive-plane programming passed;
- one user-confirmed flip passed;
- repeated page flips passed the M2 hardware ladder.

The remaining engineering step is to connect that proven flip path to the normal DRM atomic/page-flip userspace ABI. That would support standard scanout ownership, dumb buffers, page flips, and software-rendered applications through ordinary Linux display interfaces.

This is meaningful graphics progress, but the evidence boundary matters:

- **DRM/KMS display:** mode setting, vblank, scanout, buffers, page flips;
- **software rendering:** CPU draws pixels, KMS presents them;
- **SGX acceleration:** GPU firmware/power/MMU/command submission and an appropriate userspace stack.

The first two are reachable from current work. The third is not.

## Practical sequence

1. Preserve the passing IFTU/DRM M2 register and IRQ behavior in a production driver branch.
2. Implement the smallest normal atomic commit/page-flip path around the already-tested operation.
3. Add a userspace dumb-buffer flip test with bounded duration and framebuffer-console restoration.
4. Exercise repeated flips, process death, VT/fbcon handoff, and reboot cleanup.
5. Add simple software-rendered demos or SDL/KMS tooling through the Workbench workspace.
6. Keep SGX research separate so display progress cannot be blocked by the secure acceleration lane.

## What could reopen SGX work

A new lead must be materially different from the exhausted probes, such as:

- secure-world code or firmware provenance that identifies the real power/initialization ABI;
- a safe observed VitaOS transaction that can be reproduced without guessing selectors;
- documentation or reverse-engineered evidence for the VDDG revision policy;
- a non-secure register/power path newly demonstrated on hardware.

Until then, the highest-value graphics work is the normal DRM/KMS userspace path plus CPU/software rendering—not another blind attempt at GPU enablement.
