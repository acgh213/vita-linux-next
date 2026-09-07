# HyFetch PSTV validation — 2026-09-07

## Result

**PASS.** HyFetch runs on the real PlayStation TV target from the embedded
Buildroot image, using the preseeded transgender preset.

The candidate was booted from the exact OHCI kernel tip and left running after
the final validation. The prior known-good boot files were preserved locally
for rollback before staging.

## Artifact identity

- Kernel commit: `1ee3edf3c90a3b741be00216e6dc83e35614ce88`
- Kernel version: `6.12.0-g1ee3edf3c90a`
- zImage SHA-256: `70292657b82ce8c3fedc3a436f05a6f3c9aa34e5f5759812a26448cb4b141515`
- Rootfs SHA-256: `202e5f2685dac4cc25f19bfd0d47648ee7dd92d00e8af7c92430ec8dfdd7e2bb`
- PSTV DTB SHA-256: `cd2c57c72d147c046ec1eae4d650791108bddb8d995df7e3b5cbbf8d95306f98`
- HyFetch config SHA-256: `51ea8f654a461916d41972ca509f3dd672a6f8dfbd4863339cfcf329b0399735`
- HyFetch source: `1.99.0`, pinned by the Buildroot package hash

The zImage was verified to contain the byte-exact rootfs before deployment.
The uploaded zImage and PSTV DTB were accepted by the Vita loader's FTP
staging path and the loader reported `Launched.`.

## Target checks

After boot:

- `uname -r` → `6.12.0-g1ee3edf3c90a`
- online CPUs → `0-3`
- Python → `3.14.3`
- HyFetch → `/usr/bin/hyfetch`
- neofetch backend → `/usr/bin/neofetch`
- default config was present at `/root/.config/hyfetch.json`
- default `hyfetch --backend neofetch` → exit `0`
- explicit `hyfetch --backend neofetch --preset transgender` → exit `0`
- `hyfetch --help` → exit `0`
- pseudo-TTY run emitted ANSI colour sequences and identified `Host: PlayStation Vita`

The first smoke command incorrectly assumed target-side `timeout` existed;
Buildroot does not include it. The final gate bounds SSH from the host instead.
The first ANSI parser also searched before stripping interleaved colour escapes;
the corrected parser passed.
