# Vita Linux Next — progress

This file is the short current pointer. The detailed project status lives in [`docs/PROJECT-STATUS.md`](docs/PROJECT-STATUS.md); the inherited February ledger is preserved at [`docs/history/PROGRESS-2026-02-inherited.md`](docs/history/PROGRESS-2026-02-inherited.md).

## Current milestones

- Linux 6.12 boots on Vita 1000 and PSTV.
- Four-core SMP, framebuffer, input, RTC, Wi-Fi, Bluetooth, SSH, and read-only storage paths are working.
- PSTV external Type-A USB host mode and VBUS sequencing are working.
- PSTV OHCI controller access, reset, frame/SOF progression, and teardown are hardware-proven.
- An opt-in OHCI HCD enumerated a directly attached Keychron C3 Pro keyboard and drove the HDMI console.
- The OHCI/HID kernel implementation is queued for review in [`acgh213/linux_vita#9`](https://github.com/acgh213/linux_vita/pull/9).

- The full lane-by-lane ledger is [`docs/PROJECT-STATUS.md`](docs/PROJECT-STATUS.md), including DONE, IN PROGRESS, NOT IMPLEMENTED, ON HOLD, and BLOCKED work.
- HyFetch 1.99.0 is wired into the Buildroot defconfig with a preseeded transgender configuration and has passed the PSTV hardware smoke test; see [`docs/HYFETCH.md`](docs/HYFETCH.md) and the dated lab record.

## Next work

- Review and merge the opt-in OHCI HCD implementation.
- Advance this repository's kernel gitlink after the kernel merge.
- Decide the production shape for OHCI: late-ordered boot integration versus explicitly manual bring-up.
- Continue handheld USB characterization separately from the validated PSTV path.
- Keep research evidence and generated hardware artifacts in the research archive rather than the public source tree.
