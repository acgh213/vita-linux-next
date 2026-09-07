#!/bin/sh
# End-to-end fixtures for the vita-dashboard application loop.
#
# Builds the REAL sources and runs the REAL binary against a fake root: fake
# sysfs, a regular file standing in for /dev/fb0, and a FIFO standing in for the
# evdev node.  No real framebuffer or input device is touched.
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname "$0")/../.." && pwd)
SRC="$ROOT/toolkit/examples/vita-dashboard/src"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' 0 HUP INT TERM

PASS=0
fail_test() { printf 'FAIL: %s\n' "$1" >&2; exit 1; }
ok() { PASS=$((PASS + 1)); printf 'ok %s - %s\n' "$PASS" "$1"; }

CC_HOST=${CC_HOST:-cc}
CFLAGS='-std=c99 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -O1'

# shellcheck disable=SC2086
"$CC_HOST" $CFLAGS "$SRC/fb_owner.c" "$SRC/render.c" "$SRC/status.c" \
    "$SRC/input.c" "$SRC/main.c" -o "$TMP/vita-dashboard" \
    || fail_test 'dashboard failed to build with -Werror'
ok 'dashboard builds clean under -Wall -Wextra -Werror'

# ---------------------------------------------------------------- fake root --

W=64
H=32
BPP=32
STRIDE=$((W * 4))
FRAME=$((STRIDE * H))

make_root() {
    name=$1
    bind_value=$2
    phys=$3
    event_no=$4

    r=$TMP/$name
    mkdir -p "$r/dev/input" "$r/sys/class/graphics/fb0" \
             "$r/sys/class/vtconsole/vtcon1" "$r/proc" \
             "$r/sys/class/input/event$event_no/device" \
             "$r/sys/devices/system/cpu" "$r/sys/class/net/mlan0" \
             "$r/opt/vita-toolkit" "$r/proc/sys/kernel"

    printf '%s,%s\n' "$W" "$H" > "$r/sys/class/graphics/fb0/virtual_size"
    printf '%s\n' "$BPP" > "$r/sys/class/graphics/fb0/bits_per_pixel"
    printf '%s\n' "$STRIDE" > "$r/sys/class/graphics/fb0/stride"
    printf '%s\n' "$bind_value" > "$r/sys/class/vtconsole/vtcon1/bind"
    dd if=/dev/zero of="$r/dev/fb0" bs="$STRIDE" count="$H" >/dev/null 2>&1

    printf '%s\n' "$phys" > "$r/sys/class/input/event$event_no/device/phys"
    printf 'PlayStation Vita Buttons (Syscon)\n' \
        > "$r/sys/class/input/event$event_no/device/name"

    printf '0-3\n' > "$r/sys/devices/system/cpu/online"
    printf 'MemTotal: 492192 kB\nMemAvailable: 378584 kB\n' > "$r/proc/meminfo"
    printf '1234.5 4000.0\n' > "$r/proc/uptime"
    printf '6.12.0-gtest\n' > "$r/proc/sys/kernel/osrelease"
    printf 'up\n' > "$r/sys/class/net/mlan0/operstate"
    printf '44:39:c4:d6:ee:f6\n' > "$r/sys/class/net/mlan0/address"
    printf '1.2.0\n' > "$r/opt/vita-toolkit/VERSION"
    printf '/dev/loop0 /opt/vita-toolkit squashfs ro,nosuid,nodev 0 0\n' \
        > "$r/proc/mounts"
    printf '%s\n' "$r"
}

# The dashboard opens "<devroot>/event<N>"; devroot is /dev/input under the root.
make_event_node() {
    r=$1
    n=$2
    mkfifo "$r/dev/input/event$n"
}

run_dash() {
    r=$1
    shift
    # A FIFO with no writer blocks on open, so hold it open for the run.
    ( sleep 5 > "$r/dev/input/event0" & echo $! > "$TMP/holder" )
    sleep 0.1
    "$TMP/vita-dashboard" --root "$r" --machine "$@" > "$TMP/out" 2>"$TMP/err"
    rc=$?
    kill "$(cat "$TMP/holder")" 2>/dev/null || true
    return $rc
}

# ------------------------------------------- case: bounded run, fbcon bound --

R=$(make_root bound 1 vita_syscon_buttons 0)
make_event_node "$R" 0

run_dash "$R" --duration-ms 400 --input-phys vita_syscon_buttons \
    || fail_test "bounded run failed: $(cat "$TMP/err")"
out=$(cat "$TMP/out")

printf '%s\n' "$out" | grep -qx 'application=vita-dashboard' \
    || fail_test "identity line: $out"
printf '%s\n' "$out" | grep -qx "framebuffer=${W}x${H}x${BPP}" \
    || fail_test "geometry: $out"
printf '%s\n' "$out" | grep -qx "frame_bytes=$FRAME" || fail_test "frame bytes: $out"
printf '%s\n' "$out" | grep -qx 'input_identity=vita_syscon_buttons' \
    || fail_test "identity: $out"
printf '%s\n' "$out" | grep -qx 'runtime_bounded=PASS' || fail_test "bounded: $out"
printf '%s\n' "$out" | grep -qx 'status=duration_reached' || fail_test "status: $out"
ok 'a bounded run stops on its own and reports its geometry'

printf '%s\n' "$out" | grep -qx 'fbcon_was_bound=1' || fail_test "was_bound: $out"
printf '%s\n' "$out" | grep -qx 'fbcon_restored=1' || fail_test "restored: $out"
[ "$(cat "$R/sys/class/vtconsole/vtcon1/bind")" = 1 ] \
    || fail_test 'fbcon was not rebound on disk'
ok 'fbcon is unbound during the run and restored afterwards'

frames=$(printf '%s\n' "$out" | awk -F= '$1 == "frames_written" { print $2 }')
[ "$frames" -ge 1 ] || fail_test "no frames written: $out"
actual=$(wc -c < "$R/dev/fb0")
[ "$actual" -eq "$FRAME" ] \
    || fail_test "framebuffer size changed: $actual != $FRAME"
ok 'complete frames are written and the framebuffer never grows'

# The framebuffer must actually contain rendered pixels, not zeros.
nonzero=$(od -An -tu1 -v "$R/dev/fb0" | tr -s ' ' '\n' | grep -vc '^0*$' || true)
[ "$nonzero" -gt 0 ] || fail_test 'the framebuffer is still all zeros'
ok 'rendered pixel data reached the framebuffer'

# ------------------------------------- case: identity mismatch is refused ----

R2=$(make_root mismatch 1 some_other_device 0)
make_event_node "$R2" 0
if "$TMP/vita-dashboard" --root "$R2" --machine --duration-ms 200 \
        --input-phys vita_syscon_buttons > "$TMP/out" 2>"$TMP/err"; then
    fail_test 'a device with the wrong identity must be refused'
fi
grep -q 'no input device with phys=vita_syscon_buttons' "$TMP/err" \
    || fail_test "refusal message: $(cat "$TMP/err")"
[ "$(cat "$R2/sys/class/vtconsole/vtcon1/bind")" = 1 ] \
    || fail_test 'fbcon was touched despite refusing to start'
ok 'a non-matching input identity refuses to start and leaves fbcon alone'

# ------------------------------- case: identity found on a different event ---

R3=$(make_root othernode 1 vita_syscon_buttons 5)
mkfifo "$R3/dev/input/event5"
( sleep 5 > "$R3/dev/input/event5" & echo $! > "$TMP/holder5" )
sleep 0.1
"$TMP/vita-dashboard" --root "$R3" --machine --duration-ms 300 \
    --input-phys vita_syscon_buttons > "$TMP/out" 2>"$TMP/err" \
    || fail_test "event5 run failed: $(cat "$TMP/err")"
kill "$(cat "$TMP/holder5")" 2>/dev/null || true
grep -q 'input_device=.*/dev/input/event5$' "$TMP/out" \
    || fail_test "did not select event5: $(cat "$TMP/out")"
ok 'the device is selected by identity, not by a hardcoded event number'

# --------------------------------------------- case: missing framebuffer -----

R4=$(make_root nofb 1 vita_syscon_buttons 0)
make_event_node "$R4" 0
rm -f "$R4/dev/fb0"
if "$TMP/vita-dashboard" --root "$R4" --machine --duration-ms 200 \
        --input-phys vita_syscon_buttons > "$TMP/out" 2>"$TMP/err"; then
    fail_test 'a missing framebuffer must be refused'
fi
grep -q 'cannot open framebuffer' "$TMP/err" \
    || fail_test "fb refusal message: $(cat "$TMP/err")"
[ "$(cat "$R4/sys/class/vtconsole/vtcon1/bind")" = 1 ] \
    || fail_test 'fbcon was unbound despite a missing framebuffer'
ok 'a missing framebuffer refuses cleanly without touching fbcon'

# ------------------------------------------------ case: invalid duration -----

R5=$(make_root baddur 1 vita_syscon_buttons 0)
make_event_node "$R5" 0
for bad in 0 -5 999999999; do
    if "$TMP/vita-dashboard" --root "$R5" --machine --duration-ms "$bad" \
            > /dev/null 2>&1; then
        fail_test "duration $bad should be rejected"
    fi
done
ok 'invalid runtime bounds are rejected'

# --------------------------------------- case: signal restores ownership -----

R6=$(make_root signal 1 vita_syscon_buttons 0)
make_event_node "$R6" 0
( sleep 5 > "$R6/dev/input/event0" & echo $! > "$TMP/holder6" )
sleep 0.1
"$TMP/vita-dashboard" --root "$R6" --machine --forever \
    --input-phys vita_syscon_buttons > "$TMP/out" 2>"$TMP/err" &
dash_pid=$!
sleep 1
kill -TERM "$dash_pid" 2>/dev/null || true
wait "$dash_pid" 2>/dev/null || true
kill "$(cat "$TMP/holder6")" 2>/dev/null || true
[ "$(cat "$R6/sys/class/vtconsole/vtcon1/bind")" = 1 ] \
    || fail_test 'SIGTERM did not restore fbcon ownership'
grep -qx 'status=signal' "$TMP/out" || fail_test "signal status: $(cat "$TMP/out")"
grep -qx 'fbcon_restored=1' "$TMP/out" || fail_test "signal restore: $(cat "$TMP/out")"
ok 'SIGTERM restores framebuffer ownership before exiting'

# ------------------------------------------- case: frames-max bound ----------

R7=$(make_root framesmax 1 vita_syscon_buttons 0)
make_event_node "$R7" 0
( sleep 5 > "$R7/dev/input/event0" & echo $! > "$TMP/holder7" )
sleep 0.1
"$TMP/vita-dashboard" --root "$R7" --machine --forever --frames-max 3 \
    --input-phys vita_syscon_buttons > "$TMP/out" 2>"$TMP/err" \
    || fail_test "frames-max run failed: $(cat "$TMP/err")"
kill "$(cat "$TMP/holder7")" 2>/dev/null || true
grep -qx 'status=frames_reached' "$TMP/out" || fail_test "frames status: $(cat "$TMP/out")"
grep -qx 'frames_written=3' "$TMP/out" || fail_test "frame count: $(cat "$TMP/out")"
ok 'a frame-count bound stops the loop exactly on target'

printf '\nvita-dashboard: %s checks passed\n' "$PASS"
