#!/bin/sh
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname "$0")/../.." && pwd)
TOOL="$ROOT/toolkit/bin/vita-storage"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' 0 HUP INT TERM

SYS="$TMP/sys"
DEV="$TMP/dev"
PROC="$TMP/proc"
BIN="$TMP/bin"
mkdir -p "$SYS/class/block/sdb" "$SYS/class/block/mmcblk0" "$SYS/class/block/mmcblk2" \
    "$SYS/devices/usb/block/sdb/sdb1" \
    "$SYS/devices/mmc/block/mmcblk0/mmcblk0p1" \
    "$SYS/devices/mmc/mmc_host/mmc2/mmc2:0001/block/mmcblk2/mmcblk2p1" \
    "$DEV" "$PROC" "$BIN"

# The removable USB disk has one exFAT partition.
printf '1\n' > "$SYS/class/block/sdb/removable"
printf '1\n' > "$SYS/devices/usb/block/sdb/sdb1/partition"
ln -s "$SYS/devices/usb/block/sdb/sdb1" "$SYS/class/block/sdb1"
: > "$DEV/sdb1"

# The internal Vita eMMC is MMC and must stay excluded even though it has a partition.
printf '1\n' > "$SYS/devices/mmc/block/mmcblk0/mmcblk0p1/partition"
printf '0\n' > "$SYS/devices/mmc/block/mmcblk0/removable"
printf '0\n' > "$SYS/class/block/mmcblk0/removable"
ln -s "$SYS/devices/mmc/block/mmcblk0/mmcblk0p1" "$SYS/class/block/mmcblk0p1"
: > "$DEV/mmcblk0p1"

# SD2Vita appears as a real SD card behind MMC, but this controller reports its
# block device as non-removable. Its card type, not removable=1, is authoritative.
printf '1\n' > "$SYS/devices/mmc/mmc_host/mmc2/mmc2:0001/block/mmcblk2/mmcblk2p1/partition"
printf '0\n' > "$SYS/devices/mmc/mmc_host/mmc2/mmc2:0001/block/mmcblk2/removable"
printf '0\n' > "$SYS/class/block/mmcblk2/removable"
printf 'SD\n' > "$SYS/devices/mmc/mmc_host/mmc2/mmc2:0001/type"
ln -s "$SYS/devices/mmc/mmc_host/mmc2/mmc2:0001/block/mmcblk2/mmcblk2p1" "$SYS/class/block/mmcblk2p1"
: > "$DEV/mmcblk2p1"

cat > "$BIN/blkid" <<'EOF_BLKID'
#!/bin/sh
case "$1" in
    */sdb1) printf '/dev/sdb1: LABEL="New Volume" UUID="806A-41E2" TYPE="exfat"\n' ;;
    */mmcblk0p1) printf '/dev/mmcblk0p1: LABEL="Internal" UUID="1111-2222" TYPE="exfat"\n' ;;
    */mmcblk2p1) printf '/dev/mmcblk2p1: LABEL="SD2Vita" UUID="3333-4444" TYPE="exfat"\n' ;;
    *) exit 2 ;;
esac
EOF_BLKID
chmod +x "$BIN/blkid"
printf '/dev/sdb1 /mnt/vita-storage exfat ro,relatime 0 0\n/dev/mmcblk2p1 /mnt/vita-card exfat rw,relatime 0 0\n' > "$PROC/mounts"

if VITA_STORAGE_SYSFS_ROOT="$TMP/missing" "$TOOL" --machine >/dev/null 2>&1; then
    printf 'FAIL: missing test root was accepted\n' >&2
    exit 1
fi

output=$(PATH="$BIN:$PATH" \
    VITA_STORAGE_SYSFS_ROOT="$SYS" \
    VITA_STORAGE_DEV_ROOT="$DEV" \
    VITA_STORAGE_PROC_MOUNTS="$PROC/mounts" \
    VITA_STORAGE_BLKID_CMD="$BIN/blkid" \
    "$TOOL" --machine)
printf '%s\n' "$output"
printf '%s\n' "$output" | grep -F 'schema=1' >/dev/null
printf '%s\n' "$output" | grep -F 'storage_count=2' >/dev/null
printf '%s\n' "$output" | grep -F 'storage_1_device=/dev/mmcblk2p1' >/dev/null
printf '%s\n' "$output" | grep -F 'storage_1_parent=/dev/mmcblk2' >/dev/null
printf '%s\n' "$output" | grep -F 'storage_1_removable=0' >/dev/null
printf '%s\n' "$output" | grep -F 'storage_1_filesystem=exfat' >/dev/null
printf '%s\n' "$output" | grep -F 'storage_1_label=SD2Vita' >/dev/null
printf '%s\n' "$output" | grep -F 'storage_1_uuid=3333-4444' >/dev/null
printf '%s\n' "$output" | grep -F 'storage_1_mounted=1' >/dev/null
printf '%s\n' "$output" | grep -F 'storage_1_mountpoint=/mnt/vita-card' >/dev/null
printf '%s\n' "$output" | grep -F 'storage_2_device=/dev/sdb1' >/dev/null
printf '%s\n' "$output" | grep -F 'storage_2_parent=/dev/sdb' >/dev/null
printf '%s\n' "$output" | grep -F 'storage_2_removable=1' >/dev/null
printf '%s\n' "$output" | grep -F 'storage_2_filesystem=exfat' >/dev/null
printf '%s\n' "$output" | grep -F 'storage_2_label=New Volume' >/dev/null
printf '%s\n' "$output" | grep -F 'storage_2_uuid=806A-41E2' >/dev/null
printf '%s\n' "$output" | grep -F 'storage_2_mounted=1' >/dev/null
printf '%s\n' "$output" | grep -F 'storage_2_mountpoint=/mnt/vita-storage' >/dev/null
if printf '%s\n' "$output" | grep -F 'mmcblk0p1' >/dev/null; then
    printf 'FAIL: non-removable internal partition was reported\n' >&2
    exit 1
fi

pretty=$(PATH="$BIN:$PATH" \
    VITA_STORAGE_SYSFS_ROOT="$SYS" \
    VITA_STORAGE_DEV_ROOT="$DEV" \
    VITA_STORAGE_PROC_MOUNTS="$PROC/mounts" \
    VITA_STORAGE_BLKID_CMD="$BIN/blkid" \
    "$TOOL")
printf '%s\n' "$pretty" | grep -F 'Removable storage: 2' >/dev/null
printf '%s\n' "$pretty" | grep -F '/dev/mmcblk2p1' >/dev/null
printf '%s\n' "$pretty" | grep -F '/dev/sdb1' >/dev/null

printf 'vita-storage test passed\n'
