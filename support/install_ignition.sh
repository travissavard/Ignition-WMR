#!/bin/bash

# Script to install Ignition for a Windows SteamVR driver on Linux.

set -euo pipefail

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 /path/to/your/steamvr/driver"
    exit 1
fi

DRIVER_DIR="${1%/}"
DRIVER_MANIFEST="$DRIVER_DIR/driver.vrdrivermanifest"

if [ ! -f "$DRIVER_MANIFEST" ]; then
    echo "Error: driver.vrdrivermanifest not found in $DRIVER_DIR"
    exit 1
fi

echo "Found driver manifest at $DRIVER_MANIFEST"

if ! command -v jq &> /dev/null; then
    echo "Error: jq is not installed. Please install it to continue."
    echo "On Debian/Ubuntu: sudo apt-get install jq"
    echo "On Arch Linux: sudo pacman -S jq"
    echo "On Fedora: sudo dnf install jq"
    exit 1
fi

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)

echo "Reading driver name from manifest..."
if ! DRIVER_NAME=$(jq -er '.name | strings | select(length > 0)' "$DRIVER_MANIFEST"); then
    echo "Error: Could not read a non-empty driver 'name' from $DRIVER_MANIFEST"
    exit 1
fi

echo "Driver name: $DRIVER_NAME"

LINUX_BIN_DIR="$DRIVER_DIR/bin/linux64"
mkdir -p "$LINUX_BIN_DIR"

IGNITION_CONFIG_PATH="$LINUX_BIN_DIR/ignition.json"
IGNITION_DRIVER_SO="$SCRIPT_DIR/libdriver_ignition.so"
TARGET_DRIVER_SO="$LINUX_BIN_DIR/driver_$DRIVER_NAME.so"

if [ ! -f "$IGNITION_DRIVER_SO" ]; then
    echo "Error: Ignition driver not found at $IGNITION_DRIVER_SO"
    echo "Please run this script from the Ignition installation directory."
    exit 1
fi

# Oasis/WMR Linux support depends on a Wine/Proton runtime carrying WMR-specific
# USB/HID changes. The Oasis preview depot supplies that runtime launcher as
# bin/linux64/proton. Never replace it with Ignition's generic Proton helper.
DRIVER_BASENAME=$(basename "$DRIVER_DIR")
IS_WMR_OASIS=0
if [[ "${DRIVER_NAME,,}" == "oasis" || "${DRIVER_BASENAME,,}" == *"oasis"* ]]; then
    IS_WMR_OASIS=1
    if [ ! -f "$LINUX_BIN_DIR/proton" ]; then
        echo "Error: Oasis/WMR detected, but $LINUX_BIN_DIR/proton is missing."
        echo "WMR requires the WMR-compatible runtime supplied by the Oasis Linux preview package."
        echo "Switch Oasis to its preview branch in Steam, let Steam finish updating it, then rerun this installer."
        exit 1
    fi
    chmod +x "$LINUX_BIN_DIR/proton"
    echo "Detected Oasis/WMR. Preserving its WMR-compatible runtime launcher: $LINUX_BIN_DIR/proton"
fi

echo "Creating ignition.json at $IGNITION_CONFIG_PATH"
cat > "$IGNITION_CONFIG_PATH" <<EOL
{
    "server_exe": "${SCRIPT_DIR}/ignition_server.exe",
    "driver_dll": "../win64/driver_${DRIVER_NAME}.dll",
    "wine_cmd": [
        "./launch_serverhelper.sh"
    ],
    "wait_for_debugger": false
}
EOL

echo "Copying support files..."
if [ "$IS_WMR_OASIS" -eq 0 ]; then
    cp "$SCRIPT_DIR/proton" "$LINUX_BIN_DIR/proton"
    chmod +x "$LINUX_BIN_DIR/proton"
fi
cp "$SCRIPT_DIR/launch_serverhelper.sh" "$LINUX_BIN_DIR/"
cp "$SCRIPT_DIR/driver_install.sh" "$LINUX_BIN_DIR/"
cp "$SCRIPT_DIR/driver_uninstall.sh" "$LINUX_BIN_DIR/"
cp "$SCRIPT_DIR/wine_psvr2_hidraw.reg" "$LINUX_BIN_DIR/"
chmod +x "$LINUX_BIN_DIR/launch_serverhelper.sh" "$LINUX_BIN_DIR/driver_install.sh" "$LINUX_BIN_DIR/driver_uninstall.sh"

echo "Creating symbolic link for the driver..."
rm -f "$TARGET_DRIVER_SO"
ln -s "$IGNITION_DRIVER_SO" "$TARGET_DRIVER_SO"

echo "Installation complete!"
echo
if [ "$IS_WMR_OASIS" -eq 1 ]; then
    echo "Oasis/WMR runtime preserved successfully."
fi
echo "You can add the driver to SteamVR by running driver_install.sh in $LINUX_BIN_DIR"
