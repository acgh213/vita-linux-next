#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
INIT="$ROOT/buildroot-vita/board/vita/overlay/init"
TMP=$(mktemp -d "${TMPDIR:-/tmp}/vita-init-usb-test.XXXXXX")
trap 'rm -rf "$TMP"' 0 HUP INT TERM

fakebin="$TMP/bin"
devroot="$TMP/dev"
mkdir -p "$fakebin" "$devroot"

# Guard against a class of bug this gate already shipped once: a log file being
# read and written at the same time. Under set -e, a `cat` that refuses a
# self-redirect ("input file is output file") aborts the whole gate -- and
# whether it refuses depends on the coreutils version, so it passed on one host
# and failed in CI. Install a cat that always refuses, so the failure shows up
# here instead. On a host without /proc the guard is inert and harmless.
strictbin="$TMP/strictbin"
mkdir -p "$strictbin"
cat >"$strictbin/cat" <<'STRICT'
#!/bin/sh
# Refuse reading and writing the same regular file, like coreutils does when it
# takes the checking path. Fall through to the real cat otherwise.
if [ "$#" -eq 1 ] && [ -f "$1" ] && [ -e /proc/self/fd/1 ] && [ "$1" -ef /proc/self/fd/1 ]; then
	printf 'cat: %s: input file is output file\n' "$1" >&2
	exit 1
fi
exec /usr/bin/env -i PATH=/usr/bin:/bin cat "$@"
STRICT
chmod +x "$strictbin/cat"
PATH="$strictbin:$PATH"
export PATH

cat >"$fakebin/blkid" <<'EOF'
#!/bin/sh
set -eu
key=
path=
while [ "$#" -gt 0 ]; do
	case "$1" in
		-s) key=$2; shift 2 ;;
		*) path=$1; shift ;;
	esac
done
awk -F '|' -v key="$key" -v path="$path" \
	'$1 == key && $2 == path { print $3; found = 1; exit } END { if (!found) exit 1 }' \
	"$BLKID_DB"
EOF
chmod +x "$fakebin/blkid"

write_device() {
	device=$1
touch "$devroot/$device"
}
set_metadata() {
	key=$1
device=$2
value=$3
printf '%s|%s|%s\n' "$key" "$devroot/$device" "$value" >>"$BLKID_DB"
}

run_find() {
	name=$1
	expected=$2
	status=0
	output="$TMP/$name.out"
	if VITA_INIT_TEST=find_card \
		VITA_DEV_ROOT="$devroot" VITA_TEST_DEVICES=1 BLKID_DB="$BLKID_DB" \
		VITA_BLKID_CMD="$fakebin/blkid" \
		PATH="$fakebin:$PATH" "$INIT" >"$output" 2>"$TMP/$name.err"; then
		status=0
	else
		status=$?
	fi
	if [ "$expected" = FAIL ]; then
		test "$status" -ne 0 || {
			echo "$name unexpectedly selected a root" >&2
		exit 1
	}
		test ! -s "$output" || {
			echo "$name emitted a root despite failing closed" >&2
		exit 1
	}
	else
		test "$status" -eq 0 || {
			echo "$name failed to select $expected" >&2
		cat "$TMP/$name.err" >&2
		exit 1
	}
	test "$(cat "$output")" = "$devroot/$expected" || {
		echo "$name selected the wrong device: $(cat "$output")" >&2
		exit 1
	}
	fi
}

# USB VITA wins over every eligible external mmc partition, while internal
# mmcblk0 is never queried or selected.
BLKID_DB="$TMP/priority.db"
: >"$BLKID_DB"
write_device mmcblk0p1
write_device mmcblk2p1
write_device sda1
set_metadata TYPE mmcblk0p1 exfat
set_metadata LABEL mmcblk0p1 VITA
set_metadata TYPE mmcblk2p1 vfat
set_metadata LABEL mmcblk2p1 SD
set_metadata TYPE sda1 exfat
set_metadata LABEL sda1 VITA
run_find usb-vita-priority sda1

# A USB filesystem without the VITA label is not a target; the old external
# SD content path remains a valid fallback.
BLKID_DB="$TMP/fallback.db"
: >"$BLKID_DB"
write_device sda1
write_device mmcblk2p1
set_metadata TYPE sda1 exfat
set_metadata LABEL sda1 OTHER
set_metadata TYPE mmcblk2p1 vfat
set_metadata LABEL mmcblk2p1 SD
run_find external-sd-fallback mmcblk2p1

# Unsupported USB content is not accepted even when its label is VITA.
BLKID_DB="$TMP/type.db"
: >"$BLKID_DB"
write_device sda1
write_device mmcblk2p1
set_metadata TYPE sda1 ext4
set_metadata LABEL sda1 VITA
set_metadata TYPE mmcblk2p1 exfat
set_metadata LABEL mmcblk2p1 SD
run_find usb-type-filter mmcblk2p1

# Explicit USB identity prevents an old SD-root image taking over if the USB
# stick is removed for rescue. The real init must suppress its mmc fallback.
BLKID_DB="$TMP/pinned-usb.db"
: >"$BLKID_DB"
write_device sda1
write_device mmcblk2p1
set_metadata TYPE sda1 exfat
set_metadata LABEL sda1 VITA
set_metadata UUID sda1 6C9C-9FBB
set_metadata TYPE mmcblk2p1 exfat
export VITA_ROOT_USB_UUID=6C9C-9FBB
run_find pinned-usb sda1
export VITA_ROOT_USB_UUID=OTHER-UUID
run_find missing-pinned-usb FAIL
unset VITA_ROOT_USB_UUID

# Never choose arbitrarily when either target class is ambiguous.
BLKID_DB="$TMP/ambiguous-usb.db"
: >"$BLKID_DB"
write_device sda1
write_device sdb1
set_metadata TYPE sda1 exfat
set_metadata LABEL sda1 VITA
set_metadata TYPE sdb1 vfat
set_metadata LABEL sdb1 VITA
run_find ambiguous-usb FAIL

BLKID_DB="$TMP/ambiguous-mmc.db"
: >"$BLKID_DB"
write_device mmcblk2p1
write_device mmcblk3p1
set_metadata TYPE mmcblk2p1 exfat
set_metadata TYPE mmcblk3p1 vfat
run_find ambiguous-mmc FAIL

# An unambiguous USB VITA remains deterministic even if fallback media is
# ambiguous: USB is the explicitly identified target.
BLKID_DB="$TMP/usb-over-ambiguous-mmc.db"
: >"$BLKID_DB"
write_device sda1
write_device mmcblk2p1
write_device mmcblk3p1
set_metadata TYPE sda1 exfat
set_metadata LABEL sda1 VITA
set_metadata TYPE mmcblk2p1 exfat
set_metadata TYPE mmcblk3p1 vfat
run_find usb-over-ambiguous-mmc sda1

BLKID_DB="$TMP/none.db"
: >"$BLKID_DB"
run_find no-target FAIL

grep -q 'CARD_WAIT=\${VITA_CARD_WAIT:-90}' "$INIT"
grep -q 'handoff_to_newroot' "$INIT"

# dash is what actually runs this init on the console, so prefer it; fall back to
# the system sh so the gate is still runnable on a host without dash.
if command -v dash >/dev/null 2>&1; then
	dash -n "$INIT"
else
	sh -n "$INIT"
fi

mount_stub="$fakebin/mount-stub"
umount_stub="$fakebin/umount-stub"
losetup_stub="$fakebin/losetup-stub"
cat >"$mount_stub" <<'EOF'
#!/bin/sh
set -eu
printf '%s\n' "$*" >>"$MOUNT_LOG"
if [ "${MOUNT_FAIL:-}" = card ] && [ "$2" = "$CARD_MNT" ]; then
	exit 1
fi
if [ "${MOUNT_FAIL:-}" = dev ] && [ "$3" = "$NEWROOT/dev" ]; then
	exit 1
fi
exit 0
EOF
cat >"$umount_stub" <<'EOF'
#!/bin/sh
printf 'umount %s\n' "$*" >>"$MOUNT_LOG"
exit 0
EOF
cat >"$losetup_stub" <<'EOF'
#!/bin/sh
printf 'losetup %s\n' "$*" >>"$MOUNT_LOG"
exit 0
EOF
chmod +x "$mount_stub" "$umount_stub" "$losetup_stub"

run_handoff() {
	name=$1
	failure=$2
	log="$TMP/$name.log"
	status=0
	if MOUNT_LOG="$log" MOUNT_FAIL="$failure" \
		VITA_INIT_TEST=handoff VITA_MOUNT_CMD="$mount_stub" \
		VITA_UMOUNT_CMD="$umount_stub" VITA_LOSETUP_CMD="$losetup_stub" \
		VITA_CARD_MNT="$TMP/$name-card" VITA_NEWROOT="$TMP/$name-root" \
		PATH="$fakebin:$PATH" "$INIT" >/dev/null 2>"$TMP/$name.err"; then
		status=0
	else
		status=$?
	fi
	test "$status" -ne 0 || {
		echo "$name handoff failure was not propagated" >&2
		exit 1
	}
	# Do NOT cat "$log" here. The caller redirects this function's stdout into
	# the same path the stubs append to, so catting it back out is a
	# self-redirect: some cats refuse it ("input file is output file"), and
	# because this script runs under set -e that turned a green gate into a red
	# one on a newer coreutils. The log is read by the caller instead.
}

card_log="$TMP/card-failure.log"
run_handoff card-failure card >"$card_log"
! grep -q '/dev ' "$card_log"

grep -q -- '--move' "$card_log"

dev_log="$TMP/dev-failure.log"
run_handoff dev-failure dev >"$dev_log"
grep -q -- '--move /dev ' "$dev_log"
grep -q -- '--move .*dev-failure-card' "$dev_log"
grep -q 'umount ' "$dev_log"
grep -q 'losetup -d' "$dev_log"

# End-to-end main-path checks: a successful handoff reaches switch_root, while
# a /dev handoff failure returns to the initramfs rescue before switch_root.
e2e_dev="$TMP/e2e-dev"
e2e_card="$TMP/e2e-card"
e2e_root="$TMP/e2e-root"
e2e_db="$TMP/e2e-blkid.db"
e2e_log="$TMP/e2e.log"
e2e_output="$TMP/e2e.out"
mkdir -p "$e2e_dev" "$e2e_card" "$e2e_root/sbin" "$e2e_root/dev"
touch "$e2e_dev/sda1" "$e2e_card/debian-trial.img" "$e2e_root/sbin/init"
chmod +x "$e2e_root/sbin/init"
printf 'TYPE|%s|exfat\nLABEL|%s|VITA\n' "$e2e_dev/sda1" "$e2e_dev/sda1" >"$e2e_db"

select_stub="$fakebin/select-root-e2e"
e2fsck_stub="$fakebin/e2fsck-stub"
switch_stub="$fakebin/switch-root-stub"
rescue_stub="$fakebin/rescue-init-stub"
cat >"$select_stub" <<'EOF'
#!/bin/sh
printf 'normal\n'
EOF
cat >"$e2fsck_stub" <<'EOF'
#!/bin/sh
exit 0
EOF
cat >"$switch_stub" <<'EOF'
#!/bin/sh
printf 'switch_root %s\n' "$*" >>"$E2E_LOG"
exit 0
EOF
cat >"$rescue_stub" <<'EOF'
#!/bin/sh
printf 'rescue_init %s\n' "$*" >>"$E2E_LOG"
exit 0
EOF
chmod +x "$select_stub" "$e2fsck_stub" "$switch_stub" "$rescue_stub"

run_main() {
	failure=$1
	: >"$e2e_log"
	if BLKID_DB="$e2e_db" MOUNT_LOG="$e2e_log" MOUNT_FAIL="$failure" E2E_LOG="$e2e_log" \
		VITA_DEV_ROOT="$e2e_dev" VITA_TEST_DEVICES=1 VITA_BLKID_CMD="$fakebin/blkid" \
		VITA_CARD_WAIT=1 VITA_CARD_MNT="$e2e_card" VITA_NEWROOT="$e2e_root" \
		VITA_SELECT_ROOT="$select_stub" VITA_E2FSCK_CMD="$e2fsck_stub" \
		VITA_MOUNT_CMD="$mount_stub" VITA_UMOUNT_CMD="$umount_stub" \
		VITA_LOSETUP_CMD="$losetup_stub" VITA_SWITCH_ROOT_CMD="$switch_stub" \
		VITA_RESCUE_INIT="$rescue_stub" VITA_CONSOLE="$TMP/no-console" \
		VITA_LOOP_DEV="$TMP/loop7" "$INIT" >"$e2e_output" 2>"$TMP/e2e.err"; then
		status=0
	else
		status=$?
	fi
	test "$status" -eq 0 || {
		echo "main path returned $status for handoff failure=$failure" >&2
		cat "$TMP/e2e.err" >&2
		exit 1
	}
}

run_main none
grep -q -- "--move $e2e_card $e2e_root$e2e_card" "$e2e_log"
grep -q -- "--move /dev $e2e_root/dev" "$e2e_log"
grep -q '^switch_root ' "$e2e_log"

run_main dev
grep -q -- "--move /dev $e2e_root/dev" "$e2e_log"
! grep -q '^switch_root ' "$e2e_log"
grep -q '^rescue_init ' "$e2e_log"

grep -q 'handoff_to_newroot || rescue' "$INIT"

grep -q 'exec "$SWITCH_ROOT_CMD"' "$INIT"

printf 'init USB persistent-root gate: PASS\n'
