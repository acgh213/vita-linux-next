#!/bin/sh
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname "$0")/../.." && pwd)
TOOL="$ROOT/toolkit/bin/vita-status"
chmod +x "$TOOL"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' 0 HUP INT TERM

PASS=0
fail_test() { printf 'FAIL: %s\n' "$1" >&2; exit 1; }
ok() { PASS=$((PASS + 1)); printf 'ok %s - %s\n' "$PASS" "$1"; }

REQUIRED_KEYS='schema product kernel arch cpus_online memory_available_kib uptime_seconds framebuffer network_interface network_address toolkit_mounted toolkit_options toolkit_version payload_sha256 compiler transport_mode workspace'

assert_keys() {
    output=$1
    for key in $REQUIRED_KEYS; do
        printf '%s\n' "$output" | grep -q "^$key=" \
            || fail_test "missing key $key in: $output"
    done
}

# ------------------------------------------------------ full fake environment --

FAKE=$TMP/full
mkdir -p "$FAKE/proc" "$FAKE/sys/devices/system/cpu" \
         "$FAKE/sys/class/graphics/fb0" "$FAKE/sys/class/net/mlan0" \
         "$FAKE/sys/class/net/lo" "$FAKE/state" "$FAKE/payload/bin" "$FAKE/bin"

printf '0-3\n' > "$FAKE/sys/devices/system/cpu/online"
printf 'MemTotal:       492192 kB\nMemAvailable:   378584 kB\n' > "$FAKE/proc/meminfo"
printf '1234.56 4000.00\n' > "$FAKE/proc/uptime"
printf '1280,720\n' > "$FAKE/sys/class/graphics/fb0/virtual_size"
printf '32\n' > "$FAKE/sys/class/graphics/fb0/bits_per_pixel"
printf 'up\n' > "$FAKE/sys/class/net/mlan0/operstate"
printf 'unknown\n' > "$FAKE/sys/class/net/lo/operstate"
printf '1.2.0\n' > "$FAKE/payload/VERSION"
printf '#!/bin/sh\n' > "$FAKE/payload/bin/cc"
chmod +x "$FAKE/payload/bin/cc"
printf '/dev/loop0 %s squashfs ro,nosuid,nodev 0 0\n' "$FAKE/payload" > "$FAKE/proc/mounts"
{
    printf 'workspace=/mnt/vita-storage/vita-workbench\n'
    printf 'transport_mode=rw\n'
    printf 'payload_sha256=6fb29caf00b1bf614399b471e35d719dcad4bdf3a00f6a3c0eb66022aed997a2\n'
} > "$FAKE/state/session"

cat > "$FAKE/bin/ip" <<'EOF_IP'
#!/bin/sh
printf '    inet 192.168.18.43/24 brd 192.168.18.255 scope global mlan0\n'
EOF_IP
chmod +x "$FAKE/bin/ip"

run_full() {
    env VITA_STATUS_ROOT="$FAKE" \
        VITA_STATUS_PROC_MOUNTS="$FAKE/proc/mounts" \
        VITA_SESSION_STATE_DIR="$FAKE/state" \
        VITA_TOOLKIT_ROOT="$FAKE/payload" \
        VITA_STATUS_IP_CMD="$FAKE/bin/ip" \
        VITA_STATUS_UNAME_R=6.12.0-gtest \
        VITA_STATUS_UNAME_M=armv7l \
        "$TOOL" "$@"
}

out=$(run_full --machine) || fail_test "full environment failed: $out"
assert_keys "$out"
printf '%s\n' "$out" | grep -qx 'cpus_online=0-3' || fail_test "cpus: $out"
printf '%s\n' "$out" | grep -qx 'memory_available_kib=378584' || fail_test "memory: $out"
printf '%s\n' "$out" | grep -qx 'framebuffer=1280x720x32' || fail_test "framebuffer: $out"
printf '%s\n' "$out" | grep -qx 'network_interface=mlan0' || fail_test "interface: $out"
printf '%s\n' "$out" | grep -qx 'network_address=192.168.18.43/24' || fail_test "address: $out"
printf '%s\n' "$out" | grep -qx 'toolkit_mounted=1' || fail_test "toolkit: $out"
printf '%s\n' "$out" | grep -qx 'toolkit_version=1.2.0' || fail_test "version: $out"
printf '%s\n' "$out" | grep -qx 'transport_mode=rw' || fail_test "transport: $out"
printf '%s\n' "$out" | grep -qx 'uptime_seconds=1234' || fail_test "uptime: $out"
ok 'a full environment reports every field'

# The command must not write anywhere in the fake root.
before=$(find "$FAKE" | LC_ALL=C sort | cksum)
run_full --machine > /dev/null
run_full > /dev/null
after=$(find "$FAKE" | LC_ALL=C sort | cksum)
[ "$before" = "$after" ] || fail_test 'vita-status modified the filesystem'
ok 'vita-status performs no writes'

# ------------------------------------------------------- missing components --

EMPTY=$TMP/empty
mkdir -p "$EMPTY/proc" "$EMPTY/state"
: > "$EMPTY/proc/mounts"

out=$(env VITA_STATUS_ROOT="$EMPTY" \
        VITA_STATUS_PROC_MOUNTS="$EMPTY/proc/mounts" \
        VITA_SESSION_STATE_DIR="$EMPTY/state" \
        VITA_TOOLKIT_ROOT="$EMPTY/payload" \
        VITA_STATUS_IP_CMD="$EMPTY/no-such-ip" \
        "$TOOL" --machine) || fail_test "empty environment exited nonzero: $out"
assert_keys "$out"
printf '%s\n' "$out" | grep -qx 'toolkit_mounted=0' || fail_test "empty toolkit: $out"
printf '%s\n' "$out" | grep -qx 'framebuffer=UNKNOWN' || fail_test "empty framebuffer: $out"
printf '%s\n' "$out" | grep -qx 'network_interface=UNKNOWN' || fail_test "empty interface: $out"
printf '%s\n' "$out" | grep -qx 'workspace=UNKNOWN' || fail_test "empty workspace: $out"
printf '%s\n' "$out" | grep -qx 'memory_available_kib=UNKNOWN' || fail_test "empty memory: $out"
ok 'an empty environment reports UNKNOWN and still exits 0'

# ------------------------------------------------- missing network data only --

NONET=$TMP/nonet
mkdir -p "$NONET/proc" "$NONET/sys/class/net/mlan0" "$NONET/state" "$NONET/payload"
printf 'down\n' > "$NONET/sys/class/net/mlan0/operstate"
printf 'MemAvailable:   1000 kB\n' > "$NONET/proc/meminfo"
: > "$NONET/proc/mounts"
out=$(env VITA_STATUS_ROOT="$NONET" \
        VITA_STATUS_PROC_MOUNTS="$NONET/proc/mounts" \
        VITA_SESSION_STATE_DIR="$NONET/state" \
        VITA_TOOLKIT_ROOT="$NONET/payload" \
        "$TOOL" --machine) || fail_test "no-network exited nonzero: $out"
printf '%s\n' "$out" | grep -qx 'network_interface=UNKNOWN' || fail_test "down iface: $out"
printf '%s\n' "$out" | grep -qx 'network_address=UNKNOWN' || fail_test "down addr: $out"
printf '%s\n' "$out" | grep -qx 'memory_available_kib=1000' || fail_test "mem still read: $out"
ok 'a down interface does not fail the summary'

# ------------------------------------------------- unmounted toolkit, no cc --

NOTK=$TMP/notk
mkdir -p "$NOTK/proc" "$NOTK/state" "$NOTK/path"
: > "$NOTK/proc/mounts"
# A PATH that still provides the utilities vita-status needs, but no `cc`.
for util in awk head uname sed tr find sort df; do
    real=$(command -v "$util" 2>/dev/null) || continue
    ln -sf "$real" "$NOTK/path/$util"
done
out=$(env VITA_STATUS_ROOT="$NOTK" \
        VITA_STATUS_PROC_MOUNTS="$NOTK/proc/mounts" \
        VITA_SESSION_STATE_DIR="$NOTK/state" \
        VITA_TOOLKIT_ROOT="$NOTK/payload" \
        PATH="$NOTK/path" \
        "$TOOL" --machine) || fail_test "unmounted toolkit exited nonzero: $out"
printf '%s\n' "$out" | grep -qx 'toolkit_mounted=0' || fail_test "unmounted: $out"
printf '%s\n' "$out" | grep -qx 'compiler=UNKNOWN' || fail_test "compiler: $out"
ok 'an unmounted toolkit and absent compiler report UNKNOWN'

# --------------------------------------------------------- malformed geometry --

BADFB=$TMP/badfb
mkdir -p "$BADFB/proc" "$BADFB/sys/class/graphics/fb0" "$BADFB/state"
printf 'garbage\n' > "$BADFB/sys/class/graphics/fb0/virtual_size"
printf 'x\n' > "$BADFB/sys/class/graphics/fb0/bits_per_pixel"
: > "$BADFB/proc/mounts"
out=$(env VITA_STATUS_ROOT="$BADFB" \
        VITA_STATUS_PROC_MOUNTS="$BADFB/proc/mounts" \
        VITA_SESSION_STATE_DIR="$BADFB/state" \
        VITA_TOOLKIT_ROOT="$BADFB/payload" \
        "$TOOL" --machine) || fail_test "bad geometry exited nonzero: $out"
printf '%s\n' "$out" | grep -qx 'framebuffer=UNKNOWN' || fail_test "bad geometry: $out"
ok 'malformed framebuffer metadata reports UNKNOWN'

# ------------------------------------------------------------- arg handling --

if run_full --bogus >/dev/null 2>&1; then
    fail_test 'unknown option was accepted'
fi
ok 'unknown options are rejected'

printf '\nvita-status: %s checks passed\n' "$PASS"
