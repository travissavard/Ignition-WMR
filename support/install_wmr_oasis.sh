#!/bin/bash

# Configure this Ignition build for the Oasis Driver for Windows Mixed Reality.
# Requires the Oasis Linux preview package, which provides the WMR-patched
# Wine/Proton runtime and 70-wmr.rules.

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)
INSTALL_UDEV=0
REGISTER_DRIVER=0
OASIS_DIR=""

usage() {
    cat <<'USAGE'
Usage: install_wmr_oasis.sh [--install-udev] [--register-driver] [OASIS_DRIVER_DIR]

If OASIS_DRIVER_DIR is omitted, common Steam locations are searched.

Options:
  --install-udev     Install Oasis 70-wmr.rules and reload udev rules.
  --register-driver  Register Oasis as an external SteamVR driver after setup.
  -h, --help         Show this help.
USAGE
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --install-udev)
            INSTALL_UDEV=1
            ;;
        --register-driver)
            REGISTER_DRIVER=1
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        --*)
            echo "Error: unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
        *)
            if [ -n "$OASIS_DIR" ]; then
                echo "Error: only one Oasis driver directory may be supplied." >&2
                usage >&2
                exit 2
            fi
            OASIS_DIR="${1%/}"
            ;;
    esac
    shift
done

find_oasis_dir() {
    local steam_root manifest install_dir candidate
    local roots=(
        "$HOME/.local/share/Steam"
        "$HOME/.steam/steam"
        "$HOME/.steam/root"
        "/var/.steam"
        "/var/steam"
        "/var/local/Steam"
    )

    for steam_root in "${roots[@]}"; do
        manifest="$steam_root/steamapps/appmanifest_3824490.acf"
        if [ -f "$manifest" ]; then
            install_dir=$(awk -F'"' '$2 == "installdir" { print $4; exit }' "$manifest")
            if [ -n "$install_dir" ]; then
                candidate="$steam_root/steamapps/common/$install_dir"
                if [ -f "$candidate/driver.vrdrivermanifest" ]; then
                    printf '%s\n' "$candidate"
                    return 0
                fi
            fi
        fi

        candidate="$steam_root/steamapps/common/Oasis Driver for Windows Mixed Reality"
        if [ -f "$candidate/driver.vrdrivermanifest" ]; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done

    return 1
}

if [ -z "$OASIS_DIR" ]; then
    if ! OASIS_DIR=$(find_oasis_dir); then
        echo "Error: Oasis was not found in a common Steam location." >&2
        echo "Pass its driver directory explicitly, for example:" >&2
        echo "  $0 \"$HOME/.steam/steam/steamapps/common/Oasis Driver for Windows Mixed Reality\"" >&2
        exit 1
    fi
fi

MANIFEST="$OASIS_DIR/driver.vrdrivermanifest"
WIN_DRIVER="$OASIS_DIR/bin/win64/driver_oasis.dll"
WMR_RUNTIME="$OASIS_DIR/bin/linux64/proton"
UDEV_RULE="$OASIS_DIR/70-wmr.rules"

for required in "$MANIFEST" "$WIN_DRIVER" "$WMR_RUNTIME" "$UDEV_RULE"; do
    if [ ! -f "$required" ]; then
        echo "Error: required Oasis Linux preview file is missing: $required" >&2
        echo "Switch Oasis to the preview branch in Steam and let the update finish before retrying." >&2
        exit 1
    fi
done

if [ "${XDG_SESSION_TYPE:-}" = "wayland" ]; then
    echo "Warning: Wayland session detected. Oasis currently recommends X11 for Linux WMR." >&2
fi

echo "Oasis Linux preview validated at: $OASIS_DIR"
"$SCRIPT_DIR/install_ignition.sh" "$OASIS_DIR"

if [ "$INSTALL_UDEV" -eq 1 ]; then
    if [ "$(id -u)" -eq 0 ]; then
        install -m 0644 "$UDEV_RULE" /etc/udev/rules.d/70-wmr.rules
        udevadm control --reload-rules
        udevadm trigger
    elif command -v sudo >/dev/null 2>&1; then
        sudo install -m 0644 "$UDEV_RULE" /etc/udev/rules.d/70-wmr.rules
        sudo udevadm control --reload-rules
        sudo udevadm trigger
    else
        echo "Error: --install-udev requires root privileges or sudo." >&2
        exit 1
    fi
    echo "Installed 70-wmr.rules. Unplug and reconnect the WMR headset before starting SteamVR."
else
    echo "udev rule not changed. To install it, rerun with --install-udev."
fi

if [ "$REGISTER_DRIVER" -eq 1 ]; then
    "$OASIS_DIR/bin/linux64/driver_install.sh"
else
    echo "SteamVR driver not registered. To register it, rerun with --register-driver."
fi

echo "Oasis/WMR Ignition setup is complete."
