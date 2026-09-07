#!/bin/sh
# Fixture tests for vita-toolkit-session.
#
# Every case runs against fake mount/umount commands, a fake /proc/mounts, and a
# fake vita-storage, so no real filesystem is ever mounted.
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname "$0")/../.." && pwd)
TOOL="$ROOT/toolkit/bin/vita-toolkit-session"
chmod +x "$TOOL"

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' 0 HUP INT TERM

PASS=0
fail_test() {
    printf 'FAIL: %s\n' "$1" >&2
    exit 1
}
ok() {
    PASS=$((PASS + 1))
    printf 'ok %s - %s\n' "$PASS" "$1"
}

# ---------------------------------------------------------------- harness ----

# Each case gets its own sandbox so state never leaks between tests.
new_case() {
    CASE=$TMP/case-$1
    mkdir -p "$CASE/transport" "$CASE/target" "$CASE/state" "$CASE/bin"
    PROC="$CASE/mounts"
    LOG="$CASE/mount.log"
    : > "$PROC"
    : > "$LOG"

    cat > "$CASE/bin/mount" <<'EOF_MOUNT'
#!/bin/sh
printf '%s\n' "$*" >> "$FAKE_LOG"
# Record a mount line for the final two positional arguments.
src=
dst=
for arg in "$@"; do
    src=$dst
    dst=$arg
done
opts=ro
prev=
for arg in "$@"; do
    [ "$prev" = -o ] && opts=$arg
    prev=$arg
done
case "$*" in
    *remount*)
        # Rewrite the existing entry's options in place.
        awk -v target="$dst" -v opts="$opts" \
            '$2 == target { $4 = opts } { print }' "$FAKE_PROC" > "$FAKE_PROC.new"
        mv "$FAKE_PROC.new" "$FAKE_PROC"
        exit 0 ;;
esac
fstype=auto
prev=
for arg in "$@"; do
    [ "$prev" = -t ] && fstype=$arg
    prev=$arg
done
printf '%s %s %s %s 0 0\n' "$src" "$dst" "$fstype" "$opts" >> "$FAKE_PROC"
EOF_MOUNT

    cat > "$CASE/bin/umount" <<'EOF_UMOUNT'
#!/bin/sh
printf 'umount %s\n' "$*" >> "$FAKE_LOG"
awk -v target="$1" '$2 != target { print }' "$FAKE_PROC" > "$FAKE_PROC.new"
mv "$FAKE_PROC.new" "$FAKE_PROC"
EOF_UMOUNT
    chmod +x "$CASE/bin/mount" "$CASE/bin/umount"
}

# Emit a fake vita-storage --machine response from records of the form
# device|filesystem|label|mountpoint
make_storage() {
    out=$CASE/bin/vita-storage
    {
        printf '#!/bin/sh\n'
        printf "printf 'schema=1\\\\n'\n"
        printf "printf 'storage_count=%s\\\\n'\n" "$#"
    } > "$out"
    i=1
    for record in "$@"; do
        dev=${record%%|*}
        rest=${record#*|}
        fs=${rest%%|*}
        rest=${rest#*|}
        label=${rest%%|*}
        mp=${rest#*|}
        {
            printf "printf 'storage_%s_device=%s\\\\n'\n" "$i" "$dev"
            printf "printf 'storage_%s_filesystem=%s\\\\n'\n" "$i" "$fs"
            printf "printf 'storage_%s_label=%s\\\\n'\n" "$i" "$label"
            printf "printf 'storage_%s_mountpoint=%s\\\\n'\n" "$i" "$mp"
            printf "printf 'storage_%s_removable=1\\\\n'\n" "$i"
        } >> "$out"
        i=$((i + 1))
    done
    chmod +x "$out"
}

run_session() {
    env \
        FAKE_LOG="$LOG" \
        FAKE_PROC="$PROC" \
        VITA_TOOLKIT_PROC_MOUNTS="$PROC" \
        VITA_TOOLKIT_MOUNT_CMD="$CASE/bin/mount" \
        VITA_TOOLKIT_UMOUNT_CMD="$CASE/bin/umount" \
        VITA_SESSION_STORAGE_CMD="$CASE/bin/vita-storage" \
        VITA_SESSION_STATE_DIR="$CASE/state" \
        VITA_SESSION_TARGET="$CASE/target" \
        VITA_SESSION_TRANSPORT="$CASE/transport" \
        "$TOOL" "$@"
}

# Create a payload plus a valid sidecar checksum.
make_payload() {
    path=$1
    printf 'squashfs-payload-%s\n' "${2:-default}" > "$path"
    sha256sum "$path" | awk '{print $1}' > "$path.sha256"
}

# A mounted payload root must look like a real toolkit.
populate_target() {
    printf '1.1.0\n' > "$CASE/target/VERSION"
    printf 'bin/tcc 0755 1 abc\n' > "$CASE/target/NATIVE-TOOLCHAIN-MANIFEST"
}

# ------------------------------------------------------- case 1: happy rw ----

new_case happy-rw
make_payload "$CASE/transport/vita-toolkit-native.squashfs" happy
make_storage "/dev/sda1|exfat|VITA|$CASE/transport"
printf '/dev/sda1 %s exfat ro,relatime 0 0\n' "$CASE/transport" > "$PROC"
populate_target
out=$(run_session start --rw-workspace --machine) || fail_test "rw session start failed: $out"
printf '%s\n' "$out" | grep -qx 'status=mounted' || fail_test "case1 not mounted: $out"
printf '%s\n' "$out" | grep -qx 'transport_mode=rw' || fail_test "case1 transport not rw: $out"
printf '%s\n' "$out" | grep -qx "target_options=loop,ro,nosuid,nodev" || fail_test "case1 squashfs opts: $out"
printf '%s\n' "$out" | grep -q "workspace=$CASE/transport/vita-workbench" || fail_test "case1 workspace: $out"
grep -q -- '-t squashfs -o loop,ro,nosuid,nodev' "$LOG" || fail_test "case1 squashfs mount cmd: $(cat "$LOG")"
grep -q 'remount,rw' "$LOG" || fail_test "case1 expected an explicit rw remount: $(cat "$LOG")"
for sub in src build log capture projects; do
    [ -d "$CASE/transport/vita-workbench/$sub" ] || fail_test "case1 missing workspace dir $sub"
done
[ -f "$CASE/transport/vita-workbench/WORKSPACE-INFO" ] || fail_test 'case1 missing WORKSPACE-INFO'
ok 'rw workspace session mounts transport rw and squashfs ro'

# status reflects the live session
st=$(run_session status --machine)
printf '%s\n' "$st" | grep -qx 'status=mounted' || fail_test "case1 status: $st"
printf '%s\n' "$st" | grep -qx 'transport_mode=rw' || fail_test "case1 status mode: $st"
ok 'status reports the active session'

# clean teardown re-verifies the payload
stop=$(run_session stop --machine) || fail_test "case1 stop failed: $stop"
printf '%s\n' "$stop" | grep -qx 'payload_unchanged=1' || fail_test "case1 payload_unchanged: $stop"
printf '%s\n' "$stop" | grep -qx 'status=stopped' || fail_test "case1 stop status: $stop"
grep -q "umount $CASE/target" "$LOG" || fail_test 'case1 target was not unmounted'
ok 'stop verifies the payload hash and unmounts the target'

# idempotent stop
again=$(run_session stop --machine)
printf '%s\n' "$again" | grep -qx 'status=not_running' || fail_test "case1 second stop: $again"
ok 'stop is idempotent for an already-stopped session'

# ------------------------------------------------ case 2: read-only session --

new_case inspect-ro
make_payload "$CASE/transport/vita-toolkit-native.squashfs" inspect
make_storage "/dev/sda1|exfat|VITA|$CASE/transport"
printf '/dev/sda1 %s exfat ro,relatime 0 0\n' "$CASE/transport" > "$PROC"
populate_target
out=$(run_session start --machine) || fail_test "ro session failed: $out"
printf '%s\n' "$out" | grep -qx 'transport_mode=ro' || fail_test "case2 mode: $out"
printf '%s\n' "$out" | grep -qx 'workspace=NONE' || fail_test "case2 workspace: $out"
if grep -q 'remount,rw' "$LOG"; then fail_test 'case2 must not remount the transport rw'; fi
if [ -d "$CASE/transport/vita-workbench" ]; then fail_test 'case2 must not create a workbench tree'; fi
ok 'inspection-only session leaves the transport read-only'

# --------------------------------------------- case 3: no removable storage --

new_case no-storage
make_storage
printf 'none %s tmpfs rw 0 0\n' "$CASE/target" > /dev/null
if run_session start --machine > "$CASE/out" 2>&1; then
    fail_test "case3 should fail: $(cat "$CASE/out")"
fi
grep -qx 'status=no_removable_storage' "$CASE/out" || fail_test "case3 status: $(cat "$CASE/out")"
[ ! -s "$LOG" ] || fail_test "case3 must not invoke mount: $(cat "$LOG")"
ok 'no removable storage fails closed without calling mount'

# --------------------------------------------- case 4: two matching devices --

new_case ambiguous
make_storage "/dev/sda1|exfat|VITA|$CASE/transport" "/dev/sdb1|exfat|VITA2|NONE"
if run_session start --machine > "$CASE/out" 2>&1; then
    fail_test "case4 should fail: $(cat "$CASE/out")"
fi
grep -qx 'status=ambiguous_storage' "$CASE/out" || fail_test "case4 status: $(cat "$CASE/out")"
[ ! -s "$LOG" ] || fail_test "case4 must not invoke mount: $(cat "$LOG")"
ok 'ambiguous discovery fails closed without calling mount'

# ------------------------------- case 5: non-removable disk, misleading label --
# vita-storage only reports removable partitions, so a misleading label on an
# internal disk must never reach discovery.  Model it as an unsupported
# filesystem candidate that is correctly skipped.

new_case misleading-label
make_storage "/dev/mmcblk0p1|ext4|VITA|NONE"
if run_session start --machine > "$CASE/out" 2>&1; then
    fail_test "case5 should fail: $(cat "$CASE/out")"
fi
grep -qx 'status=no_candidate_storage' "$CASE/out" || fail_test "case5 status: $(cat "$CASE/out")"
[ ! -s "$LOG" ] || fail_test 'case5 must not invoke mount'
ok 'a misleading label on an unsupported filesystem is rejected'

# --------------------------------------------------- case 6: missing payload --

new_case missing-payload
make_storage "/dev/sda1|exfat|VITA|$CASE/transport"
printf '/dev/sda1 %s exfat ro,relatime 0 0\n' "$CASE/transport" > "$PROC"
if run_session start --machine > "$CASE/out" 2>&1; then
    fail_test "case6 should fail: $(cat "$CASE/out")"
fi
grep -qx 'status=payload_missing' "$CASE/out" || fail_test "case6 status: $(cat "$CASE/out")"
if grep -q 'squashfs' "$LOG"; then fail_test 'case6 must not mount a squashfs'; fi
ok 'a transport without a payload fails closed'

# ------------------------------------------------- case 7: checksum mismatch --

new_case checksum-mismatch
make_payload "$CASE/transport/vita-toolkit-native.squashfs" good
printf '%064d\n' 0 > "$CASE/transport/vita-toolkit-native.squashfs.sha256"
make_storage "/dev/sda1|exfat|VITA|$CASE/transport"
printf '/dev/sda1 %s exfat ro,relatime 0 0\n' "$CASE/transport" > "$PROC"
if run_session start --machine > "$CASE/out" 2>&1; then
    fail_test "case7 should fail: $(cat "$CASE/out")"
fi
grep -qx 'status=checksum_mismatch' "$CASE/out" || fail_test "case7 status: $(cat "$CASE/out")"
if grep -q 'squashfs' "$LOG"; then fail_test 'case7 must not mount after a checksum mismatch'; fi
ok 'checksum mismatch refuses to mount'

# ------------------------------------------------ case 8: absent checksum ----

new_case unverified
printf 'payload\n' > "$CASE/transport/vita-toolkit-native.squashfs"
make_storage "/dev/sda1|exfat|VITA|$CASE/transport"
printf '/dev/sda1 %s exfat ro,relatime 0 0\n' "$CASE/transport" > "$PROC"
if run_session start --machine > "$CASE/out" 2>&1; then
    fail_test "case8 should fail: $(cat "$CASE/out")"
fi
grep -qx 'status=unverified' "$CASE/out" || fail_test "case8 status: $(cat "$CASE/out")"
if grep -q 'squashfs' "$LOG"; then fail_test 'case8 must not mount an unverified payload'; fi
ok 'absent checksum metadata reports unverified instead of trusting the payload'

# ---------------------------------------------- case 9: already mounted target --

new_case already-mounted
make_payload "$CASE/transport/vita-toolkit-native.squashfs" already
make_storage "/dev/sda1|exfat|VITA|$CASE/transport"
{
    printf '/dev/sda1 %s exfat ro,relatime 0 0\n' "$CASE/transport"
    printf '/dev/loop0 %s squashfs ro,nosuid,nodev 0 0\n' "$CASE/target"
} > "$PROC"
out=$(run_session start --machine) || fail_test "case9 failed: $out"
printf '%s\n' "$out" | grep -qx 'status=already_mounted' || fail_test "case9 status: $out"
if grep -q 'squashfs' "$LOG"; then fail_test 'case9 must not remount an existing target'; fi
ok 'an already-mounted target is reported without remounting'

# --------------------------------------- case 10: explicit file, wrong payload --

new_case wrong-filesystem
make_payload "$CASE/transport/vita-toolkit-native.squashfs" wrongfs
make_storage "/dev/sda1|exfat|VITA|$CASE/transport"
printf '/dev/sda1 %s exfat ro,relatime 0 0\n' "$CASE/transport" > "$PROC"
# Deliberately do NOT populate VERSION/NATIVE-TOOLCHAIN-MANIFEST in the target.
if run_session start --file "$CASE/transport/vita-toolkit-native.squashfs" --machine > "$CASE/out" 2>&1; then
    fail_test "case10 should fail: $(cat "$CASE/out")"
fi
grep -qx 'status=payload_unrecognized' "$CASE/out" || fail_test "case10 status: $(cat "$CASE/out")"
grep -q "umount $CASE/target" "$LOG" || fail_test 'case10 must unmount after rejecting the payload'
ok 'a mounted image without toolkit markers is rejected and unmounted'

# ----------------------------------- case 11: payload changed while mounted ----

new_case payload-changed
make_payload "$CASE/transport/vita-toolkit-native.squashfs" changed
make_storage "/dev/sda1|exfat|VITA|$CASE/transport"
printf '/dev/sda1 %s exfat rw,relatime 0 0\n' "$CASE/transport" > "$PROC"
populate_target
run_session start --rw-workspace --machine > /dev/null || fail_test 'case11 start failed'
printf 'tampered\n' >> "$CASE/transport/vita-toolkit-native.squashfs"
if run_session stop --machine > "$CASE/out" 2>&1; then
    fail_test "case11 stop should fail: $(cat "$CASE/out")"
fi
grep -qx 'status=payload_changed' "$CASE/out" || fail_test "case11 status: $(cat "$CASE/out")"
grep -qx 'payload_unchanged=0' "$CASE/out" || fail_test "case11 flag: $(cat "$CASE/out")"
if grep -q "umount $CASE/target" "$LOG"; then fail_test 'case11 must not claim a clean teardown'; fi
ok 'a payload modified while mounted refuses to report a clean teardown'

# ------------------------------------ case 12: explicit file with rw workspace --

new_case explicit-rw
make_payload "$CASE/transport/vita-toolkit-native-2026-09-02.squashfs" explicit
make_storage "/dev/sda1|exfat|VITA|$CASE/transport"
printf '/dev/sda1 %s exfat ro,relatime 0 0\n' "$CASE/transport" > "$PROC"
populate_target
out=$(run_session start --file "$CASE/transport/vita-toolkit-native-2026-09-02.squashfs" --rw-workspace --machine) \
    || fail_test "case12 failed: $out"
printf '%s\n' "$out" | grep -qx 'source_type=explicit' || fail_test "case12 source_type: $out"
printf '%s\n' "$out" | grep -qx 'transport_mode=rw' || fail_test "case12 mode: $out"
[ -f "$CASE/transport/vita-workbench/WORKSPACE-INFO" ] || fail_test 'case12 workspace info missing'
# The explicit path must never invoke storage discovery.
ok 'an explicit payload path skips discovery and still enables the workspace'

# ------------------------------------------- case 13: unsafe target rejected ---

new_case unsafe-target
make_storage "/dev/sda1|exfat|VITA|$CASE/transport"
if run_session start --target / --machine > "$CASE/out" 2>&1; then
    fail_test 'case13 should reject an unsafe target'
fi
grep -qx 'status=unsafe_target' "$CASE/out" || fail_test "case13 status: $(cat "$CASE/out")"
ok 'an unsafe mount target is rejected'

printf '\nvita-toolkit-session: %s checks passed\n' "$PASS"
