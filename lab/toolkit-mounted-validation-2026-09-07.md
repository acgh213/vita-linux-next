# Mounted toolkit validation — PSTV — 2026-09-07

## Result

**PASS.** The existing native toolkit SquashFS was staged onto the removable
exFAT volume, mounted read-only, and exercised on the real PSTV running
`6.12.0-g1ee3edf3c90a`.

The payload remains available for the next development pass:

- exFAT storage: `/dev/sda1` → `/mnt/vita-storage` (`ro`)
- SquashFS image: `/dev/loop0` → `/opt/vita-toolkit` (`ro,nosuid,nodev`)
- payload SHA-256: `6fb29caf00b1bf614399b471e35d719dcad4bdf3a00f6a3c0eb66022aed997a2`
- payload size: `10,825,728` bytes
- host/device payload hashes matched exactly

## Toolkit checks

Machine-readable commands ran successfully from the mounted payload:

- `vita-diag --machine`
- `vita-netdiag --machine`
- `vita-storage --machine`
- `vita-usbinfo --machine`
- `vita-inputinfo --machine`

The outputs reported four online CPUs (`0-3`), a 1280×720/32bpp framebuffer,
Wi-Fi on `mlan0`, the mounted removable Kingston exFAT drive, EHCI host
controllers, and the Syscon button input device.

## Native compiler checks

The mounted toolchain reported:

```text
tcc version 0.9.28rc 2026-08-09 HEAD@2ba12e83 (ARM eabihf Linux)
```

All of these ran on the target:

- one-file `cc` compile and execution → `vita-native-ok`;
- separate compilation of two C translation units;
- object linking with `pthread` and `libm`;
- linked execution → `vita-native-linked 4.87`.

The documented mount point matters. The first trial mounted the same image at
`/opt/vita-toolkit-native`; raw TCC then looked for its compiled-in prefix and
failed to find `crt1.o`. Remounting at `/opt/vita-toolkit`, as the payload
contract specifies, made plain `cc` work without extra flags. The payload and
compiler did not need to be rebuilt.

## Framebuffer network check

`vita-fbserve.arm` was launched with `--once --bind 0.0.0.0 --port 18082`.
The Debian host fetched `/frame.bmp`, and the response validated as:

```text
HTTP body: 172854 bytes
BMP geometry: 320x180, 24-bit
startup: bind=0.0.0.0 port=18082 device=/dev/fb0 width=320 height=180 scale=4 read_only=1
```

The server exited after the single request. It opens `/dev/fb0` read-only and
does not change framebuffer ownership or contents.

## Next safe lanes

The USB storage/toolchain layer is now ready for development. The next useful
physical pass is to swap the storage drive for the Keychron keyboard (or use a
known-good powered hub), then run the bounded input watcher and interactive
input consumer against the same kernel. Keep the SquashFS mount read-only and
preserve the current HyFetch/OHCI image as the rollback pair.
