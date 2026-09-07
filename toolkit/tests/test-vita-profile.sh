#!/bin/sh
# Fixture tests for the workbench shell profile fragment.
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname "$0")/../.." && pwd)
FRAGMENT="$ROOT/buildroot-vita/board/vita/overlay/etc/profile.d/vita-toolkit.sh"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' 0 HUP INT TERM

PASS=0
fail_test() { printf 'FAIL: %s\n' "$1" >&2; exit 1; }
ok() { PASS=$((PASS + 1)); printf 'ok %s - %s\n' "$PASS" "$1"; }

[ -f "$FRAGMENT" ] || fail_test "missing profile fragment: $FRAGMENT"
sh -n "$FRAGMENT" || fail_test 'profile fragment has a syntax error'
ok 'profile fragment parses'

# The fragment must be silent and harmless when the toolkit is not mounted.
out=$(sh -c ". '$FRAGMENT'; printf 'PATH=%s\n' \"\$PATH\"; printf 'ROOTVAR=%s\n' \"\${VITA_TOOLKIT_ROOT:-NONE}\"" 2>&1)
printf '%s\n' "$out" | grep -qx 'ROOTVAR=NONE' \
    || fail_test "unmounted toolkit must not set VITA_TOOLKIT_ROOT: $out"
lines=$(printf '%s\n' "$out" | grep -v '^\(PATH=\|ROOTVAR=\)' | wc -l | tr -d ' ')
[ "$lines" -eq 0 ] || fail_test "fragment printed unexpected output: $out"
ok 'fragment is silent and sets nothing when the toolkit is absent'

# Simulate a mounted payload without touching the real /opt.
# The fragment hardcodes /opt/vita-toolkit by design (it runs on the target),
# so verify the guard logic with a copy pointed at a fake root.
FAKE=$TMP/fake
mkdir -p "$FAKE/opt/vita-toolkit/bin"
printf '1.2.0\n' > "$FAKE/opt/vita-toolkit/VERSION"
sed "s#/opt/vita-toolkit#$FAKE/opt/vita-toolkit#g" "$FRAGMENT" > "$TMP/fragment-fake.sh"

out=$(sh -c ". '$TMP/fragment-fake.sh'; printf 'PATH=%s\n' \"\$PATH\"; printf 'ROOTVAR=%s\n' \"\${VITA_TOOLKIT_ROOT:-NONE}\"")
printf '%s\n' "$out" | grep -q "ROOTVAR=$FAKE/opt/vita-toolkit" \
    || fail_test "mounted toolkit should export the root: $out"
printf '%s\n' "$out" | grep -q "$FAKE/opt/vita-toolkit/bin" \
    || fail_test "mounted toolkit should extend PATH: $out"
ok 'a mounted toolkit adds bin to PATH and exports the root'

# Sourcing twice must not duplicate the PATH entry.
out=$(sh -c ". '$TMP/fragment-fake.sh'; . '$TMP/fragment-fake.sh'; printf '%s\n' \"\$PATH\"")
count=$(printf '%s\n' "$out" | tr ':' '\n' | grep -c "^$FAKE/opt/vita-toolkit/bin$")
[ "$count" -eq 1 ] || fail_test "PATH entry duplicated $count times"
ok 'sourcing twice does not duplicate the PATH entry'

# A user-provided root must win.
out=$(sh -c "VITA_TOOLKIT_ROOT=/custom; export VITA_TOOLKIT_ROOT; . '$TMP/fragment-fake.sh'; printf '%s\n' \"\$VITA_TOOLKIT_ROOT\"")
[ "$out" = /custom ] || fail_test "fragment overwrote a user-provided root: $out"
ok 'a user-provided VITA_TOOLKIT_ROOT is preserved'

# The fragment must not define CC, TMPDIR, or aliases.
if grep -qE '^[[:space:]]*(alias|CC=|TMPDIR=)' "$FRAGMENT"; then
    fail_test 'fragment defines an alias or overrides CC/TMPDIR'
fi
ok 'fragment defines no aliases and does not override CC or TMPDIR'

printf '\nvita-profile: %s checks passed\n' "$PASS"
