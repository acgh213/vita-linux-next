# Debian root for the Vita/PSTV

What lives here is the **Debian side** of the persistent-root boot: the files that
go into the armhf ext4 image, and the SysV dependency overrides that make Debian's
shutdown order correct for a root filesystem that is a loop file on removable media.

The boot *decision* — which volume to use, whether to boot at all, how to fall back
to rescue — lives in the initramfs, in
`buildroot-vita/board/vita/overlay/init` and
`buildroot-vita/board/vita/overlay/usr/share/vita-boot/select-root`.
This directory is the other half of that agreement.

Proven on PSTV hardware 2026-09-13: Debian 12 armhf as PID 1, booted from an ext4
image on the USB stick, over Wi-Fi, with the rescue path exercised. The measured
record is `lab/usb-debian-2026-09-13/RESULTS.md` in the research repo.

## What is in here

| Path | Purpose |
|---|---|
| `packages.txt` | the package set installed into the image |
| `overlay/etc/network/interfaces` | mlan0 waits out the async mwifiex probe, then DHCP; wired NIC is hotplug-only |
| `overlay/etc/ssh/sshd_config.d/10-vita.conf` | root login by key only |
| `overlay/etc/fstab` | the root ext4 by label, fsck pass 0 (the initramfs already fsck'd it) |
| `overlay/etc/init.d/vita-boot-confirm` | closes the open boot attempt only once the system is genuinely usable |
| `overlay/etc/init.d/vita-root-sync` | after `/` is read-only, flushes the backing volume and records the result |
| `overlay/etc/init.d/vita-loopdetach` | detaches non-root card-backed loops; never the one holding `/` |
| `overlay/etc/init.d/vita-card-mounts` | exposes `/mnt/workspace` and `/opt/vita-toolkit` |
| `insserv-overrides/` | `sendsigs`, `umountroot`, `umountfs` headers that fix the stop order |

## Why the overrides exist

`insserv` reads `Required-Stop: X` as **"X stops after me"**. Debian's stock
`umountfs` declares `Required-Stop: umountroot`, so `umountfs` runs *first* — and on
this platform it cannot unmount the exFAT volume holding the root image, because the
loop device keeps it open. It then remounts that volume read-only while `/` is still
writable on a file inside it, leaving `umountroot`'s flush of `/` nowhere to land.

The overrides produce this order, verified on the console against the actual
`rc0.d` links:

```
K02sendsigs  K03umountroot  K04vita-root-sync  K06umountfs  K07halt
```

`/` goes read-only while its backing volume is still writable; the volume is then
flushed explicitly; only then does `umountfs` run.

`tests/test-debian-lifecycle.py` asserts these headers, with the measured order
recorded in the test, because `insserv` is not available on a build host.

## Reproducing the image

The image is a plain ext4 file; it is not built by Buildroot and nothing in this
repo generates it, because Debian's own package manager is what populates it.

1. **Create the ext4 file and format it.** 8 GiB is the first-trial size and grows
   later with `truncate` + `resize2fs`. `mkfs.ext4` is blocked in this agent's
   gateway, so run it yourself and label the filesystem `VITAROOT`.
2. **Bootstrap Debian armhf into it.** A `debootstrap --arch=armhf bookworm` works,
   but the proven route here was to mount the image and install into it from the
   host with `qemu-user-static` as `binfmt_misc` interpreter. Two things to know:
   - `apt-key` needs a writable `/dev`; bind-mount the host's `/dev` into the chroot
     or signature verification fails with "gpgv required but neither installed".
   - `usr/sbin/policy-rc.d` returns 101 so package postinsts do not start services
     during the install. **Delete it before first boot**, or the services never
     register.
3. **Install the packages**: `apt-get install -y --no-install-recommends $(cat packages.txt)`.
   Verify with `dpkg --audit`.
4. **Copy the overlay** in: `cp -a overlay/. <root>/`, then `chmod 0755` the
   `init.d` scripts.
5. **Copy the operator secrets** from the gitignored Buildroot overlay
   (`buildroot-vita/board/vita/local/`): `etc/wpa_supplicant.conf` (0644) and
   `root/.ssh/authorized_keys` (0600, in a 0700 directory). Also copy
   `lib/firmware/mrvl/sd8787_uapsta.bin`, `regulatory.db` and `regulatory.db.p7s`
   from the Buildroot target — the driver is built into the kernel and probes during
   the initramfs phase.
6. **Install the dependency overrides** into `<root>/etc/insserv/overrides/`, then
   register the Vita services from inside the root:
   ```sh
   insserv -v vita-loopdetach vita-root-sync vita-boot-confirm vita-card-mounts
   insserv
   ```
   Plain `insserv` alone will **not** pick up a service that has no `rc?.d` links
   yet, and will silently ignore it.
7. **Verify before deploying**, from inside the root:
   ```sh
   sshd -t && sshd -T | grep -E '^(permitrootlogin|passwordauthentication)'
   ifquery mlan0
   ls etc/rc0.d | sort        # expect the K order above
   e2fsck -fn <image>
   ```
8. **Get it onto the target.** The image is 8 GiB and compresses to ~168 MB with
   `gzip -1`. Stream it and decompress on the device rather than copying it
   uncompressed, and hash it **there** — 8 GiB through USB takes about 22 minutes,
   so a short timeout returns nothing at all. The initramfs looks for the image as
   `<card>/debian-trial.img`.

## Operating it

- **Rescue is the initramfs.** It is not a separate root: "boot rescue" means the
  initramfs declines to `switch_root`. It is boot-proven and cannot be broken by a
  bad Debian image, which is why every failure path falls through to it.
- **The operator override is a file on the card**, `vita-boot-override`, containing
  `rescue`, `normal` or `reset`. Reachable from rescue Linux, or by moving the stick
  to another machine. In rescue the card is mounted at `/mnt/vita-card`.
- **Three consecutive unconfirmed attempts** make the next boot choose rescue. A
  boot is confirmed only after PID 1, a writable root, a writable card, an SSH
  listener and a configured IPv4 default route are all verified.
- **The card is also the evidence channel.** `/mnt/vita-card/vita-boot.log` is the
  initramfs's own log and `/mnt/vita-card/boot-attempt.log` is the attempt history.
  Read them before theorising about a console that will not answer.
