# HyFetch on Vita Linux

Vita Linux Next packages [HyFetch 1.99.0](https://github.com/hykilpikonna/hyfetch)
into the Buildroot image. The target uses HyFetch's `neofetch` backend because it
is already available as a small Buildroot package; the image does not need a
second native fetch implementation or a Python package manager.

## Image configuration

`buildroot-vita/configs/vita_defconfig` enables:

- `python3`
- Python readline support (HyFetch imports `readline` on Linux)
- Buildroot `neofetch`
- the external `python-hyfetch` package

`/root/.config/hyfetch.json` is installed with a transgender preset, 8-bit colour
mode, the generic Linux logo key, and noninteractive `--stdout=off` backend
arguments. The command therefore works immediately over SSH or a serial shell:

```sh
hyfetch
hyfetch --backend neofetch --preset transgender
hyfetch --help
```

The default config is target-specific and is deliberately not copied into the
external toolkit SquashFS. HyFetch is part of the embedded rescue image so the
system identity command remains available before removable storage is mounted.

## Build and verify

From the outer project root, apply the Buildroot configuration and build the
image. On this Debian host, pass the ARM toolchain explicitly when building the
kernel:

```sh
make rootfs-config
make rootfs LINUX_VITA_DIR=<kernel-worktree>
```

Before deploying, verify the package and config are present in the generated
target tree and in the embedded archive:

```sh
grep -E '^BR2_PACKAGE_(PYTHON_HYFETCH|PYTHON3|PYTHON3_READLINE|NEOFETCH)=' \
  buildroot/.config

test -x buildroot/output/target/usr/bin/hyfetch
test -x buildroot/output/target/usr/bin/neofetch
test -x buildroot/output/target/usr/bin/python3
cmp buildroot-vita/board/vita/overlay/root/.config/hyfetch.json \
  buildroot/output/target/root/.config/hyfetch.json
```

The HyFetch 1.99.0 source tarball is pinned and hash-checked in
`buildroot-vita/package/python-hyfetch/python-hyfetch.hash`:

```text
ddeb422fd797c710f0ad37d584fac466df89e39feddeef765492b2c0b529616e
```

The target-side gate is bounded by the host and is read-only apart from the
HyFetch process itself:

```sh
timeout 20 ssh root@vita 'hyfetch --backend neofetch --preset transgender'
ssh root@vita 'hyfetch --help >/dev/null'
```

Record the kernel tip, rootfs SHA-256, and the exact HyFetch output in the lab
record. A successful Buildroot build is not a hardware pass until the command
has executed on the target.
