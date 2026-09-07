# Origins and contribution lineage

Vita Linux Next is a new public home for an ongoing project. “New repository” means a clean project boundary and a README that describes the current work; it does **not** mean erasing or taking sole credit for the work that made this possible.

## Previous repositories

- [incognitojam/vita-linux-port](https://github.com/incognitojam/vita-linux-port) — the earlier outer project and its contributor history
- [incognitojam/linux_vita](https://github.com/incognitojam/linux_vita) — the earlier kernel project
- [xerpi/linux_vita](https://github.com/xerpi/linux_vita) — the kernel lineage from which the Vita port grew
- [acgh213/vita-linux-port](https://github.com/acgh213/vita-linux-port) — Cassie's previous fork, retained as an archive and review trail

The old fork contains the detailed inherited history. Its README and status descriptions are stale, which is why this repository exists; its history and contributors are still part of the story.

## Acknowledgments

- **xerpi** — original Vita Linux port work and the low-level Vita knowledge it exposed
- **incognitojam** — the prior outer project, kernel work, integration, and the public repository that carried the project forward
- **Contributors to the predecessor repositories** — hardware research, kernel changes, tooling, documentation, and experiments recorded in their histories
- **Linux kernel and Buildroot communities** — the systems this port builds on rather than replaces
- **Vita homebrew and reverse-engineering contributors** — loader, header, firmware, and hardware documentation that make experiments possible

The current maintainer/project identity is **Cassie / acgh213**. New work is being developed in the open with explicit attribution, evidence records, and links back to the inherited projects.

## What belongs where

- Kernel implementation belongs in [`acgh213/linux_vita`](https://github.com/acgh213/linux_vita).
- Build/deployment integration belongs in this repository.
- Reverse-engineering evidence and dated hardware records belong in [`acgh213/vita-linux-research`](https://github.com/acgh213/vita-linux-research).
- Loader changes belong in [`acgh213/vita-baremetal-linux-loader`](https://github.com/acgh213/vita-baremetal-linux-loader).

That separation is deliberate: it lets a hardware result remain citable even when an implementation branch is later revised.
