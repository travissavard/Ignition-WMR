# Ignition

Allows you to run Windows-only SteamVR drivers on Linux, using Wine/Proton.

# How to use

> [!NOTE]
> If you want to get started with Playstation VR2 on Ignition, go to the [Linux support](https://github.com/BnuuySolutions/PSVR2Toolkit/wiki/Linux-support) wiki page for PSVR2Toolkit.

Currently, Ignition is packaged for Linux to run Windows drivers. The Ignition package (named Ignition-Linux-Windows) can be extracted to a folder like `/opt/ignition`, or somewhere that is at least accessible to applications running under the Steam Linux Runtime. `install_ignition.sh <driver path>` will create a new `<driver path>/bin/linux64` directory for SteamVR to load the Windows driver through Ignition. Ignition is configured by `ignition.json`, with the install script automatically filling out the config to run the Windows SteamVR driver under Proton. For ordinary drivers, you must at least install Proton from Steam, preferably **Proton Experimental**. You can use `driver_install.sh` in the `linux64` folder to install the driver for SteamVR to load.

You may also use Ignition on Windows to run Windows drivers on top of it, which is helpful for validating driver behavior due to Ignition. Currently, there is no packaging for Windows, so you must build and set it up yourself.

# Windows Mixed Reality (WMR) / Oasis

Ignition can be used with the [Oasis Driver for Windows Mixed Reality](https://store.steampowered.com/app/3824490/Oasis_Driver_for_Windows_Mixed_Reality/) on Linux. Oasis's Linux support depends on its **preview** branch, the `70-wmr.rules` udev rules file shipped with Oasis, and a WMR-compatible Wine/Proton runtime containing the required USB/HID compatibility work.

> [!IMPORTANT]
> Do not replace Oasis's `bin/linux64/proton` launcher with stock Proton. The WMR path requires the WMR-compatible runtime supplied with the Oasis Linux preview package. Ignition's installer detects Oasis and preserves that runtime instead of overwriting it.

The packaged helper can validate and configure Oasis for Ignition:

```bash
bash install_wmr_oasis.sh "/path/to/Oasis Driver for Windows Mixed Reality"
```

If Oasis is installed in a common Steam library location, the path may be omitted:

```bash
bash install_wmr_oasis.sh
```

To also install/reload Oasis's udev rule and register the SteamVR external driver:

```bash
bash install_wmr_oasis.sh --install-udev --register-driver
```

The WMR helper verifies that the Linux preview package contains the required Windows driver, WMR runtime launcher, and `70-wmr.rules` before changing the Ignition setup. It deliberately does not apply Ignition's PSVR2-specific HID registry configuration to Oasis.

Current Oasis Linux guidance recommends **X11**, not Wayland. If a Wayland session is detected, the helper warns rather than silently continuing as though the display stack were known-good.

After changing udev rules, unplug and reconnect the headset before starting SteamVR. For troubleshooting, check SteamVR's `vrserver.txt` and `vrcompositor.txt` logs.

## Current Oasis Linux limitations

Oasis's current Linux support notes list several upstream limitations. In particular, the built-in Bluetooth receiver on Samsung Odyssey+, HP Reverb G1, and HP Reverb G2 is not currently functional, so those headsets require the PC's Bluetooth receiver for controllers. Windows-key system click and HP Reverb G2 Omnicept eye tracking are also not currently supported on the Linux path.

# Supported Drivers

Known supported paths currently include:

- PlayStation VR2 using [PSVR2Toolkit](https://github.com/BnuuySolutions/PSVR2Toolkit) with its Linux/Ignition guidance.
- Windows Mixed Reality headsets using the Oasis Linux preview with its WMR-compatible Wine/Proton runtime and Ignition.

Support for additional Windows SteamVR drivers may improve over time. Driver compatibility still depends on the Windows driver's hardware APIs and on what Wine/Proton implements.

# How it works

There are three main parts to Ignition:

1. **Linux Driver Shim (`driver_ignition`)**: SteamVR on Linux loads Ignition's native Linux library (`.so`) as if it were a standard Linux driver. This proxy forwards API calls and data to and from the Windows Server.
2. **Windows Server (`ignition_server.exe`)**: Upon launch, the Linux shim starts a background Windows server process inside Wine/Proton. This server loads the target Windows driver (`.dll`) and communicates with the VR hardware through Wine/Proton.
3. **IPC Bridge & Shared Memory (`ignition_bridge`)**: An Inter-Process Communication (IPC) layer translates SteamVR API calls between the Linux shim and the Windows server. High-frequency data (such as device tracking, poses, and input events) is passed via shared memory buffers. A Wine DLL (`ignition_bridge.dll`) is loaded by the Windows side, wrapping the POSIX SHM IPC implementation.

### RPC Architecture

Ignition's RPC/IPC system is designed to be low latency and simple as possible.

* **Shared Memory Circular Buffers**: Communication between the Linux driver shim and the Windows server process is established over bi-directional shared memory circular buffers (Client-to-Server and Server-to-Client channels). Buffer access is synchronized using semaphores (or Windows Events) and atomic spinlocks.
* **RPC Layer & Object Proxies**: SteamVR interfaces (such as `IServerTrackedDeviceProvider`, `IVRDriverInput`, `IVRProperties`, `IVRSettings`, etc.) are mirrored across process boundaries using C++ RPC proxy objects derived from `RpcObject`. `RpcObject` is the base class that provides a generic interface for RPC communication.
* **Serialization & Thread Pool**: Method calls and return values are serialized into binary payloads (`RpcSerializer` / `RpcValue`) and pushed through the circular buffers. Worker threads process incoming RPC messages, invoking target driver methods and returning results back to the caller through the IPC layer.
