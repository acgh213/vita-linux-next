# Vita Linux Next — project status

**Snapshot:** 2026-09-07  
**Canonical project home:** [acgh213/vita-linux-next](https://github.com/acgh213/vita-linux-next)

## Current state

Linux 6.12 boots on both the Vita 1000 and PSTV with four Cortex-A9 CPUs, framebuffer output, core input/RTC paths, Wi-Fi, Bluetooth, SSH, and read-only storage support.

The PSTV external USB Type-A path is now hardware-verified through two milestones:

1. bus-0 OHCI controller access, reset, HCCA frame progression, and SOF IRQ delivery;
2. a real opt-in OHCI HCD registering as a Linux USB bus, accepting EHCI companion handoff, and enumerating a directly attached Keychron C3 Pro keyboard.

The keyboard bound through `usbhid`/`hid-generic`, exposed composite input devices, and drove the HDMI console. The candidate was removed after the test and the known-good B16 baseline was restored.

Primary evidence: [OHCI/HID hardware result](https://github.com/acgh213/vita-linux-research/blob/main/lab/usb-re/ohci-gate-2026-09-06/HID-HARDWARE-RESULT-2026-09-07.md).

## Integration queue

- Kernel implementation: [`acgh213/linux_vita#9`](https://github.com/acgh213/linux_vita/pull/9), open against `vita-pstv`.
- Outer integration pin: [`acgh213/vita-linux-port#29`](https://github.com/acgh213/vita-linux-port/pull/29), draft and dependent on kernel PR #9.
- New standalone home: this repository, `vita-linux-next`, initialized from a clean snapshot with a fresh history and the tested kernel candidate recorded as its `linux_vita` gitlink.

After kernel PR #9 is reviewed/merged, the standalone repository's submodule pointer should be advanced from the candidate commit to the resulting `vita-pstv` merge tip in a normal follow-up commit.

## Development principles

- hardware evidence outranks an attractive narrative;
- negative results stay recorded with their stop conditions;
- kernel, loader, rootfs, and research changes remain separate and attributable;
- generated images and local credentials do not belong in the public source history;
- every hardware candidate has a known-good rollback path.
