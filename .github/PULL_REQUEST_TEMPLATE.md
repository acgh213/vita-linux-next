## Summary

<!-- What changes, and why does it belong in this repository? -->

## Exact base and result

- Base branch/commit:
- Resulting commit:
- Kernel submodule commit, if changed:

## Host validation

<!-- List exact commands and results. A build alone is not a hardware pass. -->

- [ ] `make test`
- [ ] `make dtb`
- [ ] `make verify-dtb`
- [ ] Relevant toolkit tests

## Hardware validation

- Device/model:
- Kernel/config/DTB/rootfs/payload identity:
- Gate and timeout:
- Observed signal:
- Fault scan:
- Cleanup/rollback:

If not run on hardware, state that explicitly and describe the required gate.

## Evidence boundary

<!-- What does this result prove, and what does it deliberately not prove? -->

## Safety and compatibility

- [ ] No credentials, private keys, Wi-Fi configuration, or proprietary payloads are included.
- [ ] New hardware access is bounded and reversible.
- [ ] PSTV results are not generalized to Vita handheld hardware without a separate gate.
- [ ] Display/scanout work is not described as SGX acceleration.
