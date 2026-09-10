#!/bin/bash
set -euo pipefail

ROOT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." &> /dev/null && pwd)
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT

PKG="$TMP_DIR/package"
mkdir -p "$PKG"
cp "$ROOT_DIR/support/install_ignition.sh" "$PKG/"
cp "$ROOT_DIR/support/launch_serverhelper.sh" "$PKG/"
cp "$ROOT_DIR/support/driver_install.sh" "$PKG/"
cp "$ROOT_DIR/support/driver_uninstall.sh" "$PKG/"
cp "$ROOT_DIR/support/wine_psvr2_hidraw.reg" "$PKG/"
printf '#!/bin/bash\necho generic-proton\n' > "$PKG/proton"
printf 'shim\n' > "$PKG/libdriver_ignition.so"
printf 'server\n' > "$PKG/ignition_server.exe"
chmod +x "$PKG/install_ignition.sh" "$PKG/proton"

make_driver() {
    local path="$1"
    local name="$2"
    mkdir -p "$path/bin/win64"
    printf '{"name":"%s"}\n' "$name" > "$path/driver.vrdrivermanifest"
    printf 'driver\n' > "$path/bin/win64/driver_$name.dll"
}

# Generic drivers continue to receive Ignition's ordinary Proton helper and
# existing PSVR2 support files.
GENERIC="$TMP_DIR/generic"
make_driver "$GENERIC" "example"
"$PKG/install_ignition.sh" "$GENERIC"
grep -q 'generic-proton' "$GENERIC/bin/linux64/proton"
test -f "$GENERIC/bin/linux64/wine_psvr2_hidraw.reg"
test -L "$GENERIC/bin/linux64/driver_example.so"
grep -q 'driver_example.dll' "$GENERIC/bin/linux64/ignition.json"

# Oasis must keep the WMR-specific runtime already supplied by its Linux depot
# and must not receive the PSVR2-specific registry tweak.
OASIS="$TMP_DIR/Oasis Driver for Windows Mixed Reality"
make_driver "$OASIS" "oasis"
mkdir -p "$OASIS/bin/linux64"
printf '#!/bin/bash\necho wmr-runtime\n' > "$OASIS/bin/linux64/proton"
printf 'stale\n' > "$OASIS/bin/linux64/wine_psvr2_hidraw.reg"
chmod +x "$OASIS/bin/linux64/proton"
"$PKG/install_ignition.sh" "$OASIS"
grep -q 'wmr-runtime' "$OASIS/bin/linux64/proton"
if grep -q 'generic-proton' "$OASIS/bin/linux64/proton"; then
    echo "FAIL: Oasis WMR runtime was overwritten" >&2
    exit 1
fi
if [ -e "$OASIS/bin/linux64/wine_psvr2_hidraw.reg" ]; then
    echo "FAIL: PSVR2 registry helper leaked into the Oasis/WMR install" >&2
    exit 1
fi
test -L "$OASIS/bin/linux64/driver_oasis.so"
grep -q 'driver_oasis.dll' "$OASIS/bin/linux64/ignition.json"

# Refuse a broken Oasis install rather than silently falling back to stock
# Proton, which lacks the current WMR-specific Wine changes.
BROKEN_OASIS="$TMP_DIR/oasis-missing-runtime"
make_driver "$BROKEN_OASIS" "oasis"
if "$PKG/install_ignition.sh" "$BROKEN_OASIS" >"$TMP_DIR/broken.out" 2>&1; then
    echo "FAIL: Oasis install unexpectedly succeeded without a WMR runtime" >&2
    exit 1
fi
grep -q 'WMR-compatible runtime' "$TMP_DIR/broken.out"

# Malformed manifests should fail before mutating the driver install.
BAD_MANIFEST="$TMP_DIR/bad-manifest"
mkdir -p "$BAD_MANIFEST"
printf '{}\n' > "$BAD_MANIFEST/driver.vrdrivermanifest"
if "$PKG/install_ignition.sh" "$BAD_MANIFEST" >"$TMP_DIR/bad.out" 2>&1; then
    echo "FAIL: invalid driver manifest unexpectedly succeeded" >&2
    exit 1
fi
grep -q 'non-empty driver' "$TMP_DIR/bad.out"

# Shell syntax is part of the regression gate.
bash -n "$ROOT_DIR/support/install_ignition.sh"
bash -n "$ROOT_DIR/support/install_wmr_oasis.sh"
bash -n "$ROOT_DIR/support/launch_serverhelper.sh"
bash -n "$ROOT_DIR/tests/test_install_ignition.sh"

echo "All Ignition installer compatibility tests passed."
