# PSTV production OHCI multi-device gate — 2026-09-12

## Scope

Real-hardware follow-up on PSTV running:

```text
Linux buildroot 6.12.0-g0d1ba53a4376
```

The production `pstv-ohci` driver was loaded from boot. This session exercised several device classes and both host controllers without rebooting. The observations below came from the live kernel log and HDMI-console use.

## Proven observations

### EHCI high-speed storage

A Kingston DataTraveler 3.0 (242,417,664 × 512-byte blocks ≈ 116 GiB) enumerated on the EHCI path at USB high speed (`usb 1-1: new high-speed USB device number N using ehci-platform`) and exposed a **single** partition, `sda1`.

> **Correction — 2026-09-12.** An earlier revision of this record claimed the device "mounted `/dev/sda2` as ext4". That claim is withdrawn; it is not supported by evidence.
>
> - The device presents `sda1`, not `sda2` (`dmesg`: `sda: sda1`).
> - An ext4 mount is **impossible** on this kernel. `/proc/filesystems` on the running `6.12.0-g0d1ba53a4376` registers only `squashfs`, `vfat`, and `exfat`. The pinned `vita_defconfig` enables no ext4 support at all.
>
> The removed detail had been transcribed from a console description rather than read from a captured mount line. Which filesystem was actually mounted during that session was not retained in this evidence set, and is recorded here as **unverified** rather than guessed. The kernel-side support limit above is the durable finding and is now also documented in [`docs/WORKBENCH.md`](../docs/WORKBENCH.md).

### OHCI full-speed devices

The following devices enumerated at USB full speed (12 Mb/s) through `pstv-ohci`:

- `04d9:fc4d` USB Gaming Mouse, including mouse, keyboard, and auxiliary HID interfaces;
- `0951:16d8` Kingston HyperX Amp, whose HID consumer-control interface bound to `hid-generic`;
- `3434:0433` Keychron C3 Pro, including keyboard, auxiliary HID, and mouse interfaces.

The Keychron was used to log into the HDMI framebuffer console. It was disconnected and reconnected once; both enumerations and HID bindings succeeded.

The HyperX result proves that the physical USB audio dongle reaches the OHCI bus. It does **not** by itself prove an audio-streaming interface or playback; only its HID interface is present in this log excerpt.

### OHCI low-speed device

A PixArt Lenovo USB Optical Mouse enumerated at USB low speed (1.5 Mb/s) and bound to `hid-generic`:

```text
[38237.390149] usb 3-1: new low-speed USB device number 6 using pstv-ohci
[38237.678812] hid-generic 0003:17EF:608D.000B: input,hidraw0: USB HID v1.11 Mouse [PixArt Lenovo USB Optical Mouse] on usb-e40e0200.ohci-1/input0
```

This closes the previously open real-low-speed enumeration/binding gate.

## Boundaries

This record does not establish:

- USB Audio Class streaming or playback from the HyperX Amp;
- attached-child teardown during driver unbind;
- reboot/VBUS-notifier ordering;
- system-suspend behavior;
- cold/power-off boot behavior;
- Vita-handheld external USB behavior.

The PSTV Type-A results must not be generalized to the handheld port without a separate hardware gate.
