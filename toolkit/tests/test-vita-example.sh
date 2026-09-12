#!/bin/sh
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname "$0")/../.." && pwd)
TOOL="$ROOT/toolkit/bin/vita-example"
chmod +x "$TOOL"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' 0 HUP INT TERM

PASS=0
fail_test() { printf 'FAIL: %s\n' "$1" >&2; exit 1; }
ok() { PASS=$((PASS + 1)); printf 'ok %s - %s\n' "$PASS" "$1"; }

# A fake read-only payload holding the real curated examples.
PAYLOAD=$TMP/payload
mkdir -p "$PAYLOAD"
cp -R "$ROOT/toolkit/examples" "$PAYLOAD/examples"
# A directory without a manifest must be ignored by list.
mkdir -p "$PAYLOAD/examples/not-an-example"

WORKSPACE=$TMP/workspace
mkdir -p "$WORKSPACE/projects" "$WORKSPACE/build"

BIN=$TMP/bin
mkdir -p "$BIN"
cat > "$BIN/vita-dev" <<'EOF_DEV'
#!/bin/sh
# Fake vita-dev: records its arguments, verifies the manifest exists, and
# reports a successful build without invoking a compiler.
printf '%s\n' "$*" >> "$FAKE_DEV_LOG"
manifest=
prev=
for arg in "$@"; do
    [ "$prev" = --manifest ] && manifest=$arg
    prev=$arg
done
[ -f "$manifest" ] || { printf 'status=manifest_missing\n'; exit 1; }
printf 'schema=1\naction=test\nstatus=pass\nrun_status=0\n'
EOF_DEV
chmod +x "$BIN/vita-dev"

run() {
    env VITA_TOOLKIT_ROOT="$PAYLOAD" \
        VITA_EXAMPLE_DIR="$PAYLOAD/examples" \
        VITA_EXAMPLE_WORKSPACE="$WORKSPACE" \
        VITA_EXAMPLE_DEV_CMD="$BIN/vita-dev" \
        FAKE_DEV_LOG="$TMP/dev.log" \
        PATH="$BIN:$PATH" \
        "$TOOL" "$@"
}

: > "$TMP/dev.log"

# ------------------------------------------------------------------- list ----

out=$(run list --machine) || fail_test "list failed: $out"
printf '%s\n' "$out" | grep -q '^example_count=' || fail_test "no count: $out"
printf '%s\n' "$out" | grep -q '=hello-native$' || fail_test "hello-native missing: $out"
printf '%s\n' "$out" | grep -q '=fb-safe$' || fail_test "fb-safe missing: $out"
if printf '%s\n' "$out" | grep -q 'not-an-example'; then fail_test "listed a dir without a manifest: $out"; fi
ok 'list reports only directories carrying a vita.project'

# --------------------------------------------------------------- unknown ----

if run build no-such-example --machine > "$TMP/out" 2>&1; then
    fail_test "unknown example should fail: $(cat "$TMP/out")"
fi
grep -qx 'status=unknown_example' "$TMP/out" || fail_test "unknown status: $(cat "$TMP/out")"
ok 'an unknown example name is rejected'

# ------------------------------------------------------ traversal rejected ---

for bad in '../etc' '/etc/passwd' 'a b' 'a;b'; do
    if run build "$bad" --machine > "$TMP/out" 2>&1; then
        fail_test "name '$bad' should be rejected"
    fi
    grep -qx 'status=invalid_example_name' "$TMP/out" \
        || fail_test "name '$bad' gave: $(cat "$TMP/out")"
done
ok 'traversal and metacharacter names are rejected'

# ------------------------------------------------------------------ build ----

out=$(run build hello-native --machine) || fail_test "build failed: $out"
printf '%s\n' "$out" | grep -qx 'status=built' || fail_test "build status: $out"
printf '%s\n' "$out" | grep -qx 'dev_status=0' || fail_test "dev status: $out"
[ -f "$WORKSPACE/projects/hello-native/src/main.c" ] || fail_test 'example not copied into workspace'
[ -f "$WORKSPACE/projects/hello-native/vita.project" ] || fail_test 'manifest not copied'
[ -d "$WORKSPACE/build/hello-native" ] || fail_test 'build dir not created'
ok 'build copies the example into the workspace and invokes vita-dev'

# vita-dev must have been pointed at the WORKSPACE copy, not the payload.
grep -q "manifest $WORKSPACE/projects/hello-native/vita.project" "$TMP/dev.log" \
    || grep -q -- "--manifest $WORKSPACE/projects/hello-native/vita.project" "$TMP/dev.log" \
    || fail_test "vita-dev was not pointed at the workspace copy: $(cat "$TMP/dev.log")"
if grep -q -- "--manifest $PAYLOAD/" "$TMP/dev.log"; then
    fail_test 'vita-dev was pointed at the read-only payload'
fi
ok 'the build runs against the workspace copy, never the payload'

# ------------------------------------------------- payload stays untouched ---

payload_before=$(find "$PAYLOAD" -type f | LC_ALL=C sort | xargs cksum 2>/dev/null | cksum)
run build fb-safe --machine > /dev/null || fail_test 'fb-safe build failed'
payload_after=$(find "$PAYLOAD" -type f | LC_ALL=C sort | xargs cksum 2>/dev/null | cksum)
[ "$payload_before" = "$payload_after" ] || fail_test 'the payload was modified by a build'
ok 'building never modifies the payload'

# --------------------------------------------------------- rebuild is clean --

printf 'stale\n' > "$WORKSPACE/projects/hello-native/STALE"
run build hello-native --machine > /dev/null || fail_test 'rebuild failed'
[ ! -f "$WORKSPACE/projects/hello-native/STALE" ] \
    || fail_test 'rebuild kept a stale file from the previous copy'
ok 'a rebuild replaces the previous workspace copy'

# --------------------------------------------------------------- dev failure --

cat > "$BIN/vita-dev" <<'EOF_FAIL'
#!/bin/sh
printf 'schema=1\nstatus=fail\nrun_status=7\n'
exit 1
EOF_FAIL
chmod +x "$BIN/vita-dev"
if run build hello-native --machine > "$TMP/out" 2>&1; then
    fail_test "a failing vita-dev must propagate nonzero: $(cat "$TMP/out")"
fi
grep -qx 'status=build_failed' "$TMP/out" || fail_test "failure status: $(cat "$TMP/out")"
ok 'a failing build propagates a nonzero exit status'

# ------------------------------------------------------------------- path ----

out=$(run path fb-safe --machine) || fail_test "path failed: $out"
printf '%s\n' "$out" | grep -q "manifest=$PAYLOAD/examples/fb-safe/vita.project" \
    || fail_test "path manifest: $out"
ok 'path reports the payload manifest without copying anything'

printf '\nvita-example: %s checks passed\n' "$PASS"
