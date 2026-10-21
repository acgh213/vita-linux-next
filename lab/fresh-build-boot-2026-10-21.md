# Fresh build + boot, 2026-10-21

A complete rebuild of the consolidated tree was deployed to the PSTV and booted. This record
also contains a **negative result about the loader submodule pin** that changes what should be
merged.

## What was built

Built from `vita-linux-next` @ `main` (clean tree), 2026-10-21.

| component | source | toolchain | artifact |
|---|---|---|---|
| kernel | `linux_vita` @ `37b9348710dfe1751dae0ef0fd2954b2714d08d4` | Debian `arm-linux-gnueabihf-gcc` 14.2.0 | `zImage` 31,965,752 B `8715816a9076d7dbe4ea33ba73e054c181a74f9fa63f73fdc282468c29ae160d` |
| device tree | same | `dtc` via kernel build | `pstv.dtb` 6,133 B `72ddd25262dcbeb827059eea246fa9ea03cf56020f375665549d147106ee0317` |
| | | | `vita1000.dtb` 5,909 B `1693c9b0f68ce6c8ac9fd2772cccfab1302f7383aaff06b7b82965545533e622` |
| | | | `vita2000.dtb` 5,901 B `fee72f17a5977d3d5a24cb1ae7494954fef6ba0f09a7bdd2ccb6ae3391faf970` |
| loader | `vita-baremetal-linux-loader` (two variants, see below) | `vitasdk/vitasdk:latest` (`arm-vita-eabi-gcc`) | see table |

Build commands, verbatim:

```
make CROSS_COMPILE=arm-linux-gnueabihf- config
make -j4 CROSS_COMPILE=arm-linux-gnueabihf- build      # EXIT 0
make CROSS_COMPILE=arm-linux-gnueabihf- verify-dtb
```

The `CROSS_COMPILE` override is required on this host: the Makefile defaults to
`arm-buildroot-linux-gnueabihf-`, which is not installed here. CI's Bootlin toolchain is also
absent, so this build used Debian's cross gcc — a toolchain different from the one that
produced the previously-booted image.

Worth noting as a consistency check: the freshly built `pstv.dtb` is **byte-identical** to the
DTB already on the device (`72ddd252…`).

## The kernel is fine. The pinned loader is not.

The fresh kernel and DTBs were held constant; only the loader payload varied. Every run used
the same deployment path (`ux0:/linux/payload.bin`) and the same launch sequence
(`destroy` → `launch PLGINLDR0`).

| # | loader payload | source | result |
|---|---|---|---|
| 1 | `61303337effc77cbd7097ab28dd7688fed1894334bdc1e3e738550c13bd79b83` (106,824 B) | **`vita-linux-next` submodule pin `96c225f`** | **did not boot** — no ping, ports 22/1337/1338 all closed, 9+ minutes |
| 2 | `e9934c464e26794305b8c9b86e2a3fd79cde5c552aeadaa6d49cdb8f8c8f933d` (105,928 B) | known-good production payload | **booted**, SSH at ~240 s |
| 3 | `f5712184208467d56cc6626a9d3a17118057e126b34e63113a4f50ff8b0204ab` (105,928 B) | fresh build from `origin/vita-port` `435791f` | **booted**, SSH at ~245 s |

Run 1 is the commit `vita-linux-next` currently pins. Run 2 shows the kernel, DTBs and deploy
path were all sound at that moment. Run 3 shows a loader built **the same way, from the
integration branch**, works with that identical kernel.

Conclusion, scoped honestly to this evidence: **the pinned loader `96c225f` fails to boot on
this PSTV, and the `vita-port` tip does boot.** One trial each — this is not a claim about why,
and the failure mode (silent, no console, no network) is exactly the kind that
`docs/LOADER-OBSERVABILITY.md` says cannot be diagnosed further without a UART.

### Consequence for the repository

- The submodule pin at `96c225f` ("wip: SMP loader cache maintenance + boot refinements
  (unvalidated)") is not a safe default. Its commit message called itself unvalidated; it now
  has a hardware result, and the result is negative.
- `vita-port` (`435791f`) builds a payload that boots with the current kernel.
- The graduation PR that would land `96c225f` on `vita-port`
  (`acgh213/vita-baremetal-linux-loader#2`) should **not** be merged on the strength of it
  being already-pinned. Being pinned is not evidence.

## Final boot state

Deployed stack: fresh kernel `8715816a…`, fresh `pstv.dtb` `72ddd252…`, fresh loader
`f5712184…`.

```
$ uname -srm
Linux 6.12.0-g37b9348710df armv7l

$ nproc
4

$ free -m
              total        used        free      shared  buff/cache   available
Mem:            480          38         338           1         114         442

$ df -h /
Filesystem      Size  Used Avail Use% Mounted on
/dev/loop7      7.8G  1.5G  5.9G  20% /

$ ip -br a
mlan0            UP             192.168.18.43/24 fd24:d93:ab13:6c45:4639:c4ff:fed6:eef6/64
```

Boot time from `launch PLGINLDR0` to SSH accepting: ~245 s, consistent with the ~240 s
baseline. Two independent loader builds (run 2, run 3) landed within 5 s of each other.

## Tooling defect found during deployment

`tools/deploy-payload.py` preserved the outgoing payload by renaming it to
`payload.bin.prev-<sha12>`. When that name already existed — which happens the second time you
deploy over the same payload — the rename failed with `550 Error renaming the file` **after**
the upload but **before** the swap, leaving the deploy half-applied. The tool now appends a
counter (`-2`, `-3`, …) when the parked name is taken.

Worth flagging separately: the Makefile's `push-setup` target uploads the loader payload to
`ux0:/baremetal/payload.bin`, which is **inert** on this device series. Anyone following that
target on a PSTV deploys a payload that never runs and gets a device that appears to hang. That
is the same trap that produced the withdrawn "`0x88E` hangs the box" claim.

## Reproducing

```bash
cd vita-linux-next
git submodule update --init --recursive
make CROSS_COMPILE=arm-linux-gnueabihf- config
make -j4 CROSS_COMPILE=arm-linux-gnueabihf- build

# loader: build from vita-port, not from the pinned commit
cd vita-baremetal-linux-loader && git checkout origin/vita-port
docker run --rm -v "$PWD":/build -w /build vitasdk/vitasdk:latest make

# deploy (preserves the outgoing payload, refuses the inert path)
python3 tools/deploy-payload.py vita-baremetal-linux-loader/vita-baremetal-linux-loader.bin \
    --host <device-ip>
```
