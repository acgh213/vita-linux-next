# Contributing to Vita Linux Next

Thank you for helping make Linux on the PS Vita and PSTV less mysterious.

This is experimental hardware work. A good contribution is not only code that compiles; it says exactly what was changed, what was run, on which device, and what remains unknown.

## Repository roles

- [`acgh213/vita-linux-next`](https://github.com/acgh213/vita-linux-next) is the standalone project home: Buildroot, toolkit, integration scripts, documentation, and the pinned kernel revision.
- [`acgh213/linux_vita`](https://github.com/acgh213/linux_vita) carries kernel work. New kernel topic branches start from `vita-linux-next` unless a pull request documents another base.
- [`acgh213/vita-linux-research`](https://github.com/acgh213/vita-linux-research) preserves deeper reverse-engineering records and hardware evidence.

## Before changing code

1. Open or find an issue that states the device, current evidence, proposed scope, and safety boundary.
2. Fork the relevant integration branch rather than stacking on an unrelated topic branch.
3. Preserve the current submodule gitlink. Update it only to an exact reviewed kernel commit.
4. Keep credentials, Wi-Fi configuration, SSH keys, and proprietary material out of commits and build artifacts.

## Host gates

On Linux with an ARM hard-float cross compiler:

```bash
make test CART_CROSS_CC=arm-linux-gnueabihf-gcc
make dtb CROSS_COMPILE=arm-linux-gnueabihf-
make verify-dtb CROSS_COMPILE=arm-linux-gnueabihf-
```

When changing the Workbench or toolkit, run every executable test under `toolkit/tests/` as well. GitHub Actions exercises the Linux and macOS kernel/DTB paths.

Behavior changes and bug fixes should begin with a failing regression. Build-system changes must keep local and CI paths on the same repository-owned target and must structurally verify generated artifacts.

## Hardware evidence

Use the smallest safe gate first. A hardware report should record:

- device model and hardware/firmware distinction;
- exact kernel commit, configuration, DTB, rootfs, and payload hashes where applicable;
- command or script used;
- expected signal, observed signal, and timeout;
- relevant kernel log excerpt and fault scan;
- cleanup/rollback result;
- what the result does **not** prove.

A desktop compile is not a hardware pass. PSTV Type-A USB evidence does not establish Vita-handheld USB behavior. Display scanout does not establish SGX acceleration.

Do not submit guessed secure-world SMC experiments. GPU work needs a new provenance-backed lever and a reversible, bounded hardware plan.

## Pull requests

Keep commits reviewable and use a conventional subject such as `fix:`, `feat:`, `docs:`, `test:`, or a kernel-style subsystem prefix. In the PR body, include:

- summary and motivation;
- exact base and resulting commit;
- host validation;
- hardware validation, or an explicit `not run on hardware` statement;
- safety/rollback notes;
- open gates and dependencies.

Do not rewrite hardware-observed history to make a result look cleaner. Negative results and regressions are useful project evidence.

## Licensing

Original code and documentation in this outer repository are available under the [MIT License](LICENSE). Submodules, imported packages, firmware, generated sysroots, and other third-party components retain their own licenses. Kernel contributions must follow the licensing and contribution requirements of the Linux kernel tree in which they are submitted.
