#!/bin/bash

set -e

unset SteamGameId

# PSVR2 uses this registry tweak for Sense controllers. WMR/Oasis installs do
# not copy the file, so their WMR-specific Wine/Proton runtime is left alone.
if [ -f ./wine_psvr2_hidraw.reg ]; then
    ./proton run reg import ./wine_psvr2_hidraw.reg
fi

./proton run "$@"
