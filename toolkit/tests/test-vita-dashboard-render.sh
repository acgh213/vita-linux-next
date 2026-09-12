#!/bin/sh
# Renderer fixtures: clipping, colour order, and out-of-bounds safety.
# Runs under ASAN/UBSAN when available so a bounds bug fails loudly.
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname "$0")/../.." && pwd)
SRC="$ROOT/toolkit/examples/vita-dashboard/src/render.c"
FIXTURE="$ROOT/toolkit/tests/vita-dashboard-render-fixture.c"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' 0 HUP INT TERM

fail_test() { printf 'FAIL: %s\n' "$1" >&2; exit 1; }

CC_HOST=${CC_HOST:-cc}
CFLAGS='-std=c99 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -O1'

SAN=
printf 'int main(void){return 0;}\n' > "$TMP/santest.c"
# shellcheck disable=SC2086
if "$CC_HOST" $CFLAGS -fsanitize=address,undefined "$TMP/santest.c" \
        -o "$TMP/santest" 2>/dev/null; then
    SAN='-fsanitize=address,undefined -fno-omit-frame-pointer'
    printf 'sanitizers: enabled\n'
else
    printf 'sanitizers: unavailable, continuing without\n'
fi
rm -f "$TMP/santest" "$TMP/santest.c"

# shellcheck disable=SC2086
"$CC_HOST" $CFLAGS $SAN "$FIXTURE" "$SRC" -o "$TMP/renderfixture" \
    || fail_test 'render fixture failed to build'

"$TMP/renderfixture" || fail_test 'render fixture reported failures'

printf 'vita-dashboard-render: fixtures passed\n'
