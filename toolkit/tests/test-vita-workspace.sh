#!/bin/sh
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname "$0")/../.." && pwd)
TOOL="$ROOT/toolkit/bin/vita-workspace"
chmod +x "$TOOL"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' 0 HUP INT TERM

PASS=0
fail_test() { printf 'FAIL: %s\n' "$1" >&2; exit 1; }
ok() { PASS=$((PASS + 1)); printf 'ok %s - %s\n' "$PASS" "$1"; }

STATE=$TMP/state
mkdir -p "$STATE"

run() {
    env VITA_SESSION_STATE_DIR="$STATE" \
        VITA_WORKSPACE_VOLATILE_ROOT="$TMP/volatile/vita-workbench" \
        VITA_TOOLKIT_ROOT="$TMP/payload" \
        "$TOOL" "$@"
}

# ---------------------------------------------------- volatile (no session) --

out=$(run path --machine) || fail_test "volatile path failed: $out"
printf '%s\n' "$out" | grep -qx 'mode=volatile' || fail_test "expected volatile: $out"
printf '%s\n' "$out" | grep -qx 'status=absent' || fail_test "expected absent: $out"
ok 'no session resolves a volatile workspace'

out=$(run init --machine) || fail_test "volatile init failed: $out"
printf '%s\n' "$out" | grep -qx 'status=ready' || fail_test "init status: $out"
for sub in src build log capture projects; do
    [ -d "$TMP/volatile/vita-workbench/$sub" ] || fail_test "missing $sub"
done
ok 'init creates every workspace subdirectory'

out=$(run clean --machine) || fail_test "volatile clean failed: $out"
printf '%s\n' "$out" | grep -qx 'status=removed' || fail_test "clean status: $out"
[ ! -d "$TMP/volatile/vita-workbench" ] || fail_test 'clean left the workspace behind'
ok 'clean removes a volatile workspace without --persistent'

# ------------------------------------------------------ persistent session --

PERSIST=$TMP/transport/vita-workbench
mkdir -p "$TMP/transport"
{
    printf 'payload=%s\n' "$TMP/transport/payload.squashfs"
    printf 'payload_sha256=%064d\n' 0
    printf 'target=%s\n' "$TMP/payload"
    printf 'transport=%s\n' "$TMP/transport"
    printf 'transport_mode=rw\n'
    printf 'workspace=%s\n' "$PERSIST"
    printf 'transport_mounted_by_session=0\n'
} > "$STATE/session"

out=$(run path --machine) || fail_test "persistent path failed: $out"
printf '%s\n' "$out" | grep -qx 'mode=persistent' || fail_test "expected persistent: $out"
printf '%s\n' "$out" | grep -q "workspace=$PERSIST" || fail_test "workspace path: $out"
ok 'an rw session resolves the persistent USB workspace'

run init --machine > /dev/null || fail_test 'persistent init failed'
printf 'marker\n' > "$PERSIST/projects/marker.txt"

if run clean --machine > "$TMP/out" 2>&1; then
    fail_test "persistent clean without the flag should fail: $(cat "$TMP/out")"
fi
grep -qx 'status=persistent_requires_flag' "$TMP/out" || fail_test "clean guard: $(cat "$TMP/out")"
[ -f "$PERSIST/projects/marker.txt" ] || fail_test 'guarded clean destroyed data'
ok 'clean refuses to remove a persistent workspace without --persistent'

out=$(run clean --persistent --machine) || fail_test "explicit clean failed: $out"
printf '%s\n' "$out" | grep -qx 'status=removed' || fail_test "explicit clean status: $out"
[ ! -d "$PERSIST" ] || fail_test 'explicit clean did not remove the workspace'
[ -d "$TMP/transport" ] || fail_test 'clean removed the transport volume itself'
ok 'clean --persistent removes only the workspace, not the volume'

# ------------------------------------------- ro session falls back volatile --

{
    printf 'transport_mode=ro\n'
    printf 'workspace=NONE\n'
} > "$STATE/session"
out=$(run path --machine) || fail_test "ro session path failed: $out"
printf '%s\n' "$out" | grep -qx 'mode=volatile' || fail_test "ro session should be volatile: $out"
ok 'an inspection-only session falls back to a volatile workspace'

# ---------------------------------------------- never inside the payload ----

{
    printf 'transport_mode=rw\n'
    printf 'workspace=%s/vita-workbench\n' "$TMP/payload"
} > "$STATE/session"
if run init --machine > "$TMP/out" 2>&1; then
    fail_test "workspace inside the payload should fail: $(cat "$TMP/out")"
fi
grep -qx 'status=unsafe_workspace' "$TMP/out" || fail_test "payload guard: $(cat "$TMP/out")"
ok 'a workspace inside the toolkit payload is rejected'

# ------------------------------------------ unexpected workspace name guard --

{
    printf 'transport_mode=rw\n'
    printf 'workspace=%s/notaworkbench\n' "$TMP/transport"
} > "$STATE/session"
mkdir -p "$TMP/transport/notaworkbench"
if run clean --persistent --machine > "$TMP/out" 2>&1; then
    fail_test "unexpected workspace name should fail: $(cat "$TMP/out")"
fi
grep -qx 'status=unexpected_workspace_name' "$TMP/out" || fail_test "name guard: $(cat "$TMP/out")"
[ -d "$TMP/transport/notaworkbench" ] || fail_test 'guard removed the directory anyway'
ok 'clean refuses a path that is not named vita-workbench'

printf '\nvita-workspace: %s checks passed\n' "$PASS"
