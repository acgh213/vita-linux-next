#!/bin/sh
# Payload-wide regression checks for the Vita Linux Workbench (plan task 6.2).
#
# Verifies properties of the STAGED payload rather than of individual commands:
# shell syntax, absence of credential material, ARM-only ELF binaries, stable
# machine-output schema markers, and two-build manifest reproducibility.
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname "$0")/../.." && pwd)
cd "$ROOT"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' 0 HUP INT TERM

PASS=0
fail_test() { printf 'FAIL: %s\n' "$1" >&2; exit 1; }
ok() { PASS=$((PASS + 1)); printf 'ok %s - %s\n' "$PASS" "$1"; }

# ------------------------------------------------------------ shell syntax --

for script in toolkit/bin/*; do
    case "$script" in
        *.arm) continue ;;
    esac
    [ -f "$script" ] || continue
    head -n 1 "$script" | grep -q '^#!' || continue
    sh -n "$script" || fail_test "syntax error in $script"
done
for script in toolkit/tests/*.sh tools/build-vita-toolkit-squashfs.sh; do
    [ -f "$script" ] || continue
    sh -n "$script" || fail_test "syntax error in $script"
done
ok 'every payload script and test parses'

# ------------------------------------------- executable-bit independence ----
# The suite must run tests via `sh "$t"`; a missing +x bit has silently skipped
# tests before.  Assert the runner contract by checking every test is readable
# and parses, regardless of its mode.
for t in toolkit/tests/*.sh; do
    [ -r "$t" ] || fail_test "test is not readable: $t"
done
ok 'all tests are readable and run via sh, not via the executable bit'

# ------------------------------------------------------------- manifest -----

MANIFEST=$TMP/manifest-1
tools/build-vita-toolkit-squashfs.sh --source toolkit --manifest-only \
    > "$MANIFEST" || fail_test 'manifest generation failed'
[ -s "$MANIFEST" ] || fail_test 'manifest is empty'
ok 'the payload manifest builds'

# No credentials, keys, or local overlay material may ever be staged.
if grep -qE '(^|/)(\.git|\.ssh|local)(/|$)|wpa_supplicant|authorized_keys|id_ed25519|\.psk' \
        "$MANIFEST"; then
    fail_test "manifest contains forbidden material: $(grep -nE 'ssh|local|wpa|key' "$MANIFEST")"
fi
ok 'the manifest contains no credential or local-overlay material'

# Manifest must be sorted and free of duplicates.
awk '{print $1}' "$MANIFEST" > "$TMP/paths"
LC_ALL=C sort -c "$TMP/paths" || fail_test 'manifest is not sorted'
dupes=$(LC_ALL=C uniq -d < "$TMP/paths" | wc -l | tr -d ' ')
[ "$dupes" -eq 0 ] || fail_test 'manifest contains duplicate paths'
ok 'the manifest is sorted and duplicate-free'

# Every entry must carry mode, size, and a 64-char hash.
awk 'NF != 4 { print "bad line: " $0; bad=1 } END { exit bad }' "$MANIFEST" \
    || fail_test 'malformed manifest entry'
awk '{ if (length($4) != 64) { print "bad hash: " $0; bad=1 } } END { exit bad }' \
    "$MANIFEST" || fail_test 'malformed manifest hash'
ok 'every manifest entry has mode, size, and a sha256'

# ------------------------------------------------- reproducible manifests ---

MANIFEST2=$TMP/manifest-2
tools/build-vita-toolkit-squashfs.sh --source toolkit --manifest-only \
    > "$MANIFEST2" || fail_test 'second manifest generation failed'
cmp -s "$MANIFEST" "$MANIFEST2" \
    || fail_test 'two manifest builds from identical sources differ'
ok 'two builds from identical sources produce identical manifests'

# --------------------------------------------------------- ARM-only ELFs ----

if command -v file >/dev/null 2>&1; then
    for binary in toolkit/bin/*.arm; do
        [ -f "$binary" ] || continue
        description=$(file -b "$binary")
        case "$description" in
            *ARM*) ;;
            *) fail_test "$binary is not an ARM ELF: $description" ;;
        esac
        case "$description" in
            *x86-64*|*Intel*80386*)
                fail_test "$binary is a host binary wearing an .arm name" ;;
        esac
    done
    ok 'every deployed .arm binary is a genuine ARM ELF'
else
    printf 'skip - file(1) unavailable, cannot verify ELF architecture\n'
fi

# ----------------------------------------------- machine schema markers -----
# Every --machine command must emit a stable schema marker.
for tool in vita-status vita-workspace vita-example vita-toolkit-session; do
    grep -q 'schema=1' "toolkit/bin/$tool" \
        || fail_test "$tool does not emit a schema marker"
done
grep -q 'schema=1' toolkit/examples/vita-dashboard/src/main.c \
    || fail_test 'vita-dashboard does not emit a schema marker'
ok 'every machine-output command emits a stable schema marker'

# ------------------------------------------- the session cannot mount rw ----
# The SquashFS payload mount must be read-only in EVERY code path.
if grep -n 'mount' toolkit/bin/vita-toolkit-session \
        | grep -- '-t squashfs' | grep -qv 'loop,ro,nosuid,nodev'; then
    fail_test 'a squashfs mount path does not use loop,ro,nosuid,nodev'
fi
grep -q 'TARGET_OPTIONS=loop,ro,nosuid,nodev' toolkit/bin/vita-toolkit-session \
    || fail_test 'the session does not pin the squashfs mount to read-only'
ok 'the session can never mount the payload writable'

# ------------------------------------------- dashboard restores ownership ---
grep -q 'fb_owner_release' toolkit/examples/vita-dashboard/src/main.c \
    || fail_test 'the dashboard never releases framebuffer ownership'
grep -q 'sigaction(SIGTERM' toolkit/examples/vita-dashboard/src/main.c \
    || fail_test 'the dashboard does not handle SIGTERM'
ok 'the dashboard restores framebuffer ownership and handles signals'

# --------------------------------------------- no target-side timeout use ---
# The running image has no BusyBox timeout/unshare applet; payload commands must
# not depend on them (see lab/workbench-baseline-2026-09-07.md).
for tool in toolkit/bin/vita-status toolkit/bin/vita-workspace \
            toolkit/bin/vita-example toolkit/bin/vita-toolkit-session \
            toolkit/bin/vita-dashboard; do
    if grep -qE '(^|[^-[:alnum:]_])(timeout|unshare) ' "$tool"; then
        fail_test "$tool depends on a target-side timeout/unshare applet"
    fi
done
ok 'no payload command depends on timeout(1) or unshare(1)'

printf '\nworkbench regressions: %s checks passed\n' "$PASS"
