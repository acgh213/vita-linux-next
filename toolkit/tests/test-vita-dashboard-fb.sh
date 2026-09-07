#!/bin/sh
# Framebuffer ownership fixtures for vita-dashboard.
#
# Compiles the production fb_owner.c against fake sysfs trees and a regular
# file standing in for /dev/fb0.  Nothing here touches a real framebuffer.
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname "$0")/../.." && pwd)
SRC="$ROOT/toolkit/examples/vita-dashboard/src/fb_owner.c"
FIXTURE="$ROOT/toolkit/tests/vita-dashboard-fb-fixture.c"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' 0 HUP INT TERM

fail_test() { printf 'FAIL: %s\n' "$1" >&2; exit 1; }

CC_HOST=${CC_HOST:-cc}
command -v "$CC_HOST" >/dev/null 2>&1 || fail_test "no host compiler: $CC_HOST"

CFLAGS='-std=c99 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -O1'
# Sanitizers when available; they catch out-of-bounds frame handling.
SAN=
printf 'int main(void){return 0;}\n' > "$TMP/santest.c"
if "$CC_HOST" $CFLAGS -fsanitize=address,undefined "$TMP/santest.c" -o "$TMP/santest" 2>/dev/null; then
    SAN='-fsanitize=address,undefined -fno-omit-frame-pointer'
    printf 'sanitizers: enabled\n'
else
    printf 'sanitizers: unavailable, continuing without\n'
fi
rm -f "$TMP/santest" "$TMP/santest.c"

# shellcheck disable=SC2086 # CFLAGS/SAN are intentional word lists.
"$CC_HOST" $CFLAGS $SAN "$FIXTURE" "$SRC" -o "$TMP/fbfixture" \
    || fail_test 'fixture failed to build'

# Build a fake root. bind_value: 1 = fbcon bound, 0 = already unbound.
make_root() {
    name=$1
    bind_value=$2
    size_line=$3
    bpp_line=$4
    stride_line=$5
    fb_bytes=$6

    r=$TMP/$name
    mkdir -p "$r/dev" "$r/sys/class/graphics/fb0" "$r/sys/class/vtconsole/vtcon1"
    printf '%s\n' "$size_line" > "$r/sys/class/graphics/fb0/virtual_size"
    printf '%s\n' "$bpp_line" > "$r/sys/class/graphics/fb0/bits_per_pixel"
    if [ -n "$stride_line" ]; then
        printf '%s\n' "$stride_line" > "$r/sys/class/graphics/fb0/stride"
    fi
    printf '%s\n' "$bind_value" > "$r/sys/class/vtconsole/vtcon1/bind"
    if [ "$fb_bytes" -gt 0 ]; then
        dd if=/dev/zero of="$r/dev/fb0" bs=1024 count=$((fb_bytes / 1024)) \
            >/dev/null 2>&1
    else
        : > "$r/dev/fb0"
    fi
    printf '%s\n' "$r"
}

# A small but realistic geometry keeps the fixture fast: 64x16x32, stride 256.
FRAME=$((256 * 16))

printf '\n--- case: fbcon bound ---\n'
R=$(make_root bound 1 '64,16' 32 256 "$FRAME")
"$TMP/fbfixture" "$R" bound || fail_test 'bound lifecycle failed'
[ "$(cat "$R/sys/class/vtconsole/vtcon1/bind")" = 1 ] \
    || fail_test 'bind state not restored on disk'
# The written frame must be exactly one frame long, not longer.
actual=$(wc -c < "$R/dev/fb0")
[ "$actual" -eq "$FRAME" ] || fail_test "framebuffer grew: $actual != $FRAME"

printf '\n--- case: fbcon already unbound ---\n'
R=$(make_root unbound 0 '64,16' 32 256 "$FRAME")
"$TMP/fbfixture" "$R" unbound || fail_test 'unbound lifecycle failed'
[ "$(cat "$R/sys/class/vtconsole/vtcon1/bind")" = 0 ] \
    || fail_test 'an originally-unbound console was rebound'

printf '\n--- case: malformed geometry ---\n'
R=$(make_root badgeo 1 'garbage' 32 256 "$FRAME")
"$TMP/fbfixture" "$R" bad-geometry || fail_test 'bad geometry case failed'
[ "$(cat "$R/sys/class/vtconsole/vtcon1/bind")" = 1 ] \
    || fail_test 'fbcon was unbound despite a failed probe'

printf '\n--- case: short framebuffer ---\n'
R=$(make_root shortfb 1 '64,16' 32 256 1024)
"$TMP/fbfixture" "$R" short-fb || fail_test 'short framebuffer case failed'
[ "$(cat "$R/sys/class/vtconsole/vtcon1/bind")" = 1 ] \
    || fail_test 'fbcon was unbound for a short framebuffer'

printf '\n--- case: stride falls back to packed width ---\n'
R=$(make_root nostride 1 '64,16' 32 '' $((64 * 4 * 16)))
"$TMP/fbfixture" "$R" bound || fail_test 'stride fallback case failed'

printf '\nvita-dashboard-fb: all framebuffer ownership fixtures passed\n'
