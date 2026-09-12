#!/bin/sh
# Post-build script for Vita rootfs
# Runs after packages are installed and overlays are applied, but before
# the rootfs image is created.
# $1 = path to target rootfs (e.g. output/target)

set -e

TARGET_DIR="$1"

# --- Vita eMMC mountpoint directories ---
for part in os0 vs0 sa0 tm0 vd0 ud0 pd0 ur0 emmc; do
    mkdir -p "${TARGET_DIR}/mnt/${part}"
done

# --- OpenSSH 10.x persistent image requirements (pitfall #10) ---
# Source-perm trust is not enough: a copy stage was observed widening 0600
# overlay keys to 0644 in the image. /run/sshd is intentionally created at
# boot by S49sshd-runtime because /run is volatile runtime state.
mkdir -p "${TARGET_DIR}/var/empty"
chown 0:0 "${TARGET_DIR}/var/empty" 2>/dev/null || true
chmod 711 "${TARGET_DIR}/var/empty"
for key in "${TARGET_DIR}"/etc/ssh/ssh_host_*_key; do
    [ -f "$key" ] && chmod 600 "$key"
done
if [ -f "${TARGET_DIR}/root/.ssh/authorized_keys" ]; then
    chmod 600 "${TARGET_DIR}/root/.ssh/authorized_keys"
fi

# --- Game-card workbench mountpoints ---
# S06toolkit creates these at runtime too, but pre-creating them keeps the
# mount table predictable and lets a rescue shell mount by hand without mkdir.
mkdir -p "${TARGET_DIR}/mnt/vita-card"
mkdir -p "${TARGET_DIR}/mnt/workspace"
mkdir -p "${TARGET_DIR}/opt/vita-toolkit"

# --- opkg state ---
# opkg refuses to run without its lists dir and lock dir present.
mkdir -p "${TARGET_DIR}/var/lib/opkg/lists"
mkdir -p "${TARGET_DIR}/usr/lib/opkg"
