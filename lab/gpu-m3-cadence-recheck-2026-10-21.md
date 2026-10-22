# Display cadence recheck after the timer correction

The previous M3 summary repeated an old ~52.1 Hz display figure without reconciling it with
the completed A9 timer fix. That was incorrect. No clock register or timer DT change was made
for this recheck, and no modeset/page flip was requested.

Running kernel: `6.12.0-g4c46f62c3178`. Live dmesg still identifies sched_clock at 166 MHz and
`lpj=1656675`; source has periphclk=165667500 for both A9 timers. The timer fix remains closed.

Read-only sampling over SSH paired `/proc/uptime` with `iftu_state`:

- Start uptime 1216.20 s; hardware vblank IRQ count 72793.
- End uptime 1226.24 s; hardware vblank IRQ count 73395.
- 602 interrupts over about 10.04 s: approximately 59.96 Hz, i.e. **about 60 Hz**, not 52 Hz.
- Selector and cached state stayed at zero; no flip was pending.

Raw capture: `gates/2026-10-21-m3-corrected-boot/cadence-recheck.txt` relative to this lab directory.
Reads are sequential and `/proc/uptime` has coarse decimal resolution. This is enough to reject
the old 52 Hz pacing assumption, not to choose an exact fractional pixel-clock declaration.
Do not claim 59.96 Hz as a precision result or infer HDMI PLL programming from it.

## Implications and next steps

1. Longer paired monotonic/IRQ measurements on the corrected kernel, checking uncertainty and
   comparing the already-evidenced inherited timing totals. No clock writes or timer changes.
2. Reconcile the DRM fixed-mode metadata (currently 64473 kHz, totals 1650x750) with this cadence.
   The old metadata is not a reason to retune physical hardware. Keep metadata-only correction
   distinct from changing display timing or clocks.
3. Derive the next flip deadlines from current cadence, not the old mode declaration. The B2
   successful event took 49.589437 ms, which remains below five frames at approximately 60 Hz
   (about 83 ms), but the instrument's original 95.970407 ms budget was too permissive. Record
   this correction rather than relabeling that old budget as accurate.
4. Run a bounded sustained-presentation gate and separate CPU-copy time, vblank waiting and
   userspace event latency. The one-shot 50 ms observation does not establish the cause or maximum
   sustainable rate. Report a cadence failure as a failure, not as a clock problem by default.
5. Validate teardown and recovery, then restore a framebuffer console through the explicitly
   gated fbdev takeover. M3 is not complete without those separate gates.
