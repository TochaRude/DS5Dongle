# DS5Dongle — TochaRude Fork

**Turn a Raspberry Pi Pico 2 W (or compatible RP2350 board) into a low-latency wireless DualSense / DualSense Edge USB dongle.**

![Platform](https://img.shields.io/badge/Platform-Pico%202%20W%20%2F%20RP2350-C51A4A?style=flat-square)
![Upstream](https://img.shields.io/badge/Fork%20of-awalol%2FDS5Dongle-blue?style=flat-square)
![Prebuilt](https://img.shields.io/badge/Prebuilt-ds5--bridge.uf2-brightgreen?style=flat-square)
![License](https://img.shields.io/badge/License-MIT-lightgrey?style=flat-square)

> [!IMPORTANT]
> **Independent Fork Notice:**
> This repository ([TochaRude/DS5Dongle](https://github.com/TochaRude/DS5Dongle)) is an **independent fork** of [awalol/DS5Dongle](https://github.com/awalol/DS5Dongle), developed and maintained by **TochaRude** with the assistance of **Antigravity**.
> It is **not affiliated with, endorsed by, or developed in cooperation with** the original author (`awalol`). Please open any issues or questions regarding this fork in [this repository's Issues](https://github.com/TochaRude/DS5Dongle/issues) rather than upstream.

[What This Project Is For](#what-this-project-is-for) · [Differences from Upstream](#how-this-fork-differs-from-upstream) · [Quick Start](#quick-start) · [Controls & Configuration](#controls--configuration) · [Build from Source](#supported-boards--build-from-source) · [Credits & License](#credits--license)

---

## What This Project Is For

On PC, Sony's DualSense and DualSense Edge controllers only expose their full feature set — **HD haptics, adaptive triggers, built-in speaker, 3.5 mm headset audio, and controller microphone** — over a **wired USB** connection, not over standard PC Bluetooth.

**DS5Dongle** bridges that gap in hardware: the Raspberry Pi Pico 2 W pairs with your DualSense wirelessly over Bluetooth and presents itself to the host computer as a **native wired USB DualSense controller** — with no special PC drivers required and running at the **stock 150 MHz clock** (no overclocking needed).

### Core Capabilities

- **Native Wired USB Emulation** — appears to Windows/Linux/macOS games as a wired DualSense (`VID 054C` / `PID 0CE6`, or DualSense Edge)
- **Full HD Haptics & Adaptive Triggers** — low-latency wireless bridge for game-driven tactile feedback and trigger effects
- **USB Audio Output & Input** — supports the controller speaker, 3.5 mm headset jack, and controller microphone as standard USB audio devices
- **Zero-Overclock Real-Time Audio Path** — Opus codec, resampler, and hot-path Bluetooth/USB handlers execute from RAM at 150 MHz
- **Physical & Software Management** — BOOTSEL button shortcuts for pairing, switching controllers, rebooting, and entering bootloader mode
- **Optional Shortcuts & USB Wake** — configurable button chords (up to 9 slots) and optional Wake-on-PS from S3 sleep

---

## How This Fork Differs from Upstream

While [awalol/DS5Dongle](https://github.com/awalol/DS5Dongle) provides the core wireless-to-USB bridge architecture, this fork keeps that lean single-controller design intact while focusing on **firmware hardening, bug fixes, memory/FIFO stability, deterministic real-time behavior, and ready-to-flash Pico 2 W binaries** — developed and verified iteratively with **Antigravity**.

| Area | Upstream (`awalol/DS5Dongle`) | This Fork (`TochaRude/DS5Dongle`) |
| --- | --- | --- |
| **Bluetooth & CYW43 Stability** | Periodic SPI LED updates & heap-allocated feature-report map | Eliminates CYW43 LED SPI flooding, fixes `update_state` sequence counter handling, removes link-key log leaks, and replaces the heap map with a static cache + non-reentrant control TX queue |
| **Audio, Haptics & Core Safety** | Baseline FIFO sizes and unsynchronized hot-path helpers | Deeper audio/haptics FIFOs to reduce underruns, cross-core atomics, safe UAC1 volume clamping, guarded report buffer bounds, and gated hot-path `printf` calls |
| **Config Integrity & Commands** | Config loaded without CRC verification; incomplete `0xF6` bootloader cmd | Verifies flash config CRC32 on load, implements `0xF6` Reboot-to-BOOTSEL (`0x04`), and enables `polling_rate_mode=3` (Real-Time) in `tools/config_tool.py` |
| **RAM Relocation & Builds** | Standard CMake Opus/SDK relocation rules | Hardened CMake Opus/SDK RAM relocations (`src/ram_mem.c`, `CMakeLists.txt`) and updated `tools/build-windows.ps1` & `tools/build-macos.sh` |
| **Prebuilt Pico 2 W Binary** | Provided via GitHub Releases / Actions | Includes a prebuilt, verified Release UF2 ([`ds5-bridge.uf2`](./ds5-bridge.uf2)) for Raspberry Pi Pico 2 W |

---

## Quick Start

### 1. Get the Firmware

- Download the prebuilt Raspberry Pi Pico 2 W firmware from [Releases](https://github.com/TochaRude/DS5Dongle/releases) or directly from [`ds5-bridge.uf2`](./ds5-bridge.uf2) in this repository.
- For other boards (Waveshare RP2350B-Plus-W, Pico W) or custom flags, see [Supported Boards & Build from Source](#supported-boards--build-from-source).

### 2. Flash the Pico 2 W

1. Hold the **BOOTSEL** button on the Pico 2 W while plugging it into your PC via USB.
2. Wait for the `RP2350` USB mass-storage drive to appear.
3. Drag and drop `ds5-bridge.uf2` onto the drive — the Pico reboots automatically once flashing completes.

> [!TIP]
> Once the firmware is running, you can re-enter bootloader mode without unplugging by **triple-clicking BOOTSEL**, clicking **Reboot to Bootloader** in the web config, or using `tools/reboot_bootsel.py`.

### 3. Pair Your DualSense

1. Make sure the Pico 2 W is plugged in (if no controller is paired/connected, it automatically scans for pairing).
2. Put your DualSense into pairing mode by holding **PS + Create (Share)** until the light bar blinks rapidly.
3. Wait a few seconds for the connection to complete.

> [!NOTE]
> By design, the Pico **only exposes the DualSense USB device to your PC after a controller connects** over Bluetooth. If the controller does not pair right away, short-press the BOOTSEL button to start a fresh 30-second scan or replug the Pico while the controller is blinking.

---

## Controls & Configuration

### BOOTSEL Button Controls (While Running)

| Action | Behavior |
| --- | --- |
| **Single click** | **Connected:** disconnects the current controller while keeping its pairing.<br>**Disconnected:** starts a 30-second scan to pair a new controller. |
| **Double click** | **Soft reboot** — restarts the firmware and drops the active BT link. |
| **Triple click** | **Reboot into BOOTSEL** — mounts the USB bootloader drive for flashing a new `.uf2`. |
| **Long press (~1.5 s)** | **Forget all pairings** — clears stored pairings and blacklists auto-reconnect across power cycles (onboard LED flashes 6× to confirm). |

### Status LED & Low-Battery Warning

- **Solid ON:** normal connected operation (unless `disable_pico_led` is enabled).
- **1 Hz Blink:** the connected DualSense battery is **≤ 10%** and not charging. This warning overrides `disable_pico_led` until the controller is plugged in or its battery recovers (disable at compile time with `-DENABLE_BATT_LED=OFF`).

### Web Configurator & CLI

- **Web Config (WebHID):** [https://ds5-dev.awalol.eu.org](https://ds5-dev.awalol.eu.org) or [https://ds5.awalol.eu.org](https://ds5.awalol.eu.org)
- **CLI Tool:** `python tools/config_tool.py` (requires Python + HID package)

> Some USB interface settings (polling rate, controller mode, USB serial number, keyboard interface, or USB wake) require clicking **Reconnect USB** (or replugging the dongle) to take effect.

<details>
<summary><strong>Audio, Microphone & Volume Lock Modes (<code>lock_volume</code>)</strong></summary>

- **Microphone level in Windows:** The controller microphone appears as `"Headset Microphone"`. Windows often defaults capture volume to `0` on first install — raise the recording level in Windows Sound Settings if it seems silent.
- **Bluetooth audio caveat:** Minor audio stuttering can occasionally occur depending on Bluetooth RF environment and host load.
- **Volume Lock (`lock_volume`) modes:**
  - `0` — **Off:** both game HID reports and system USB Audio controls can change volume/mute.
  - `1` — **Game / Software:** ignores volume and mute changes inside HID output reports; OS USB Audio controls still work.
  - `2` — **UAC (System Volume):** ignores USB Audio volume/mute changes (including Windows device volume); game HID controls still work. Set your desired level before enabling mode `2`.

</details>

<details>
<summary><strong>Custom Button Chords & Shortcuts (Up to 9 Slots)</strong></summary>

Trigger syntax:
- `Button` — single tap
- `Button*2` — double tap within 250 ms
- `Button1+Button2` — triggers when both buttons are held
- `Button1+Button2*2` — chord formed twice

Actions:
- **Keyboard:** modifiers + key (e.g. `Win+Shift+S`, `Win+PrintScreen`, `Win+G`, `F13`, or raw HID usage IDs like `0x16`).
- **Media:** `VolumeUp`, `VolumeDown`, `Mute`, `PlayPause`, `Next`, `Prev`, `Stop`, `BrightnessUp`, `BrightnessDown`.
- **Firmware:** `bt_disconnect` (does not require USB keyboard interfaces).

Enable `enable_keyboard=1` (and reconnect USB) for keyboard/media actions:

```sh
python tools/config_tool.py shortcut
python tools/config_tool.py shortcut 1=PS+Create:Win+PrintScreen
python tools/config_tool.py shortcut 2=PS+UP:VolumeUp
python tools/config_tool.py shortcut 3=PS+DOWN:VolumeDown
python tools/config_tool.py shortcut 4=PS:Win+G
python tools/config_tool.py shortcut 6=PS*2:Win+Tab
python tools/config_tool.py shortcut 1=off
```

*Limitations:* Trigger buttons are not swallowed (games/overlays still see them); two D-pad directions cannot form a chord; Windows media volume actions adjust the DualSense audio endpoint rather than the active default output device.

</details>

<details>
<summary><strong>Optional Wake-on-PS (S3 Sleep Only)</strong></summary>

Enabling **Wake PC from sleep on PS button** in the config (and reconnecting USB) enumerates a boot keyboard + consumer control HID interface and advertises USB remote wakeup. Pressing a controller button while the host is in **S3 sleep** injects an **F15** keypress to wake the PC within ~1 s. *(Modern Standby / S0ix is not supported; check `powercfg /a` for `Standby (S3)`.)*

1. In **Device Manager**, enable *"Allow this device to wake the computer"* on the new **HID Keyboard Device** and its parent **USB Composite Device**.
2. Verify with `powercfg /devicequery wake_armed`.
3. If `SelectiveSuspendEnabled` was not set by Windows on the audio interface (`MI_00`), apply it via elevated PowerShell (`PID_0CE6` for DualSense, `PID_0DF2` for DualSense Edge) and reconnect USB:

```powershell
Get-ChildItem 'HKLM:\SYSTEM\CurrentControlSet\Enum\USB\VID_054C&PID_0CE6&MI_00' | ForEach-Object {
  New-ItemProperty "$($_.PSPath)\Device Parameters" SelectiveSuspendEnabled -Value 1 -PropertyType DWord -Force
}
```

</details>

---

## Supported Boards & Build from Source

| Board | CMake Flag | Support Level |
| --- | --- | --- |
| **Raspberry Pi Pico 2 W** | *(default)* | Full support (haptics, adaptive triggers, speaker, 3.5 mm jack, mic) |
| **Waveshare RP2350B-Plus-W** | `-DWAVESHARE_RP2350B_PLUS_W_BUILD=ON` | Full RP2350B support (16 MB flash, USB-C) |
| **Raspberry Pi Pico W** | `-DPICO_W_BUILD=ON` | Haptics support only (no speaker/audio) |

### Windows (Automated Script)

Run from the repository root in PowerShell (automatically handles dependencies and builds `ds5-bridge.uf2`):

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\build-windows.ps1
```

### macOS

```sh
tools/build-macos.sh
```

### Manual CMake Build (Linux / Cross-Platform)

Requires **Pico SDK 2.3.1** with its **TinyUSB** submodule pinned to commit `98741b55959564746998d0453c32d87064629d7c`:

```sh
git -C <sdk>/lib/tinyusb fetch --depth 1 origin 98741b55959564746998d0453c32d87064629d7c
git -C <sdk>/lib/tinyusb checkout --detach 98741b55959564746998d0453c32d87064629d7c
git submodule update --init --recursive

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DPICO_SDK_PATH=<sdk>
cmake --build build --target ds5-bridge
```

Optional build flags: `-DVERSION=<tag>` (custom version string reported in WebHID), `-DENABLE_BATT_LED=OFF`, `-DWAVESHARE_RP2350B_PLUS_W_BUILD=ON`, `-DPICO_W_BUILD=ON`.

---

## Related Community Forks & References

### Ecosystem Forks

- **[awalol/DS5Dongle](https://github.com/awalol/DS5Dongle)** — Original upstream project
- **[loteran/DS5Dongle](https://github.com/loteran/DS5Dongle)** — Audio-to-haptics conversion (simulated rumble from game audio)
- **[SundayMoments/DS5_Bridge](https://github.com/SundayMoments/DS5_Bridge)** — Extended customization (lighting, trigger strength, remapping, shortcuts)
- **[MarcelineVPQ/DS5Dongle-OLED-Edition](https://github.com/MarcelineVPQ/DS5Dongle-OLED-Edition)** — Optional 128×64 OLED display add-on & diagnostics UI
- **[artzox/DS5Dongle](https://github.com/artzox/DS5Dongle)** — Expanded simulated haptics, anti-aliasing, gyro-aiming & profiles
- **[snipem/DS4Dongle](https://github.com/snipem/DS4Dongle)** — DualShock 4 adaptation for Pico 2 W

### Technical References

- [rafaelvaloto/Pico_W-Dualsense](https://github.com/rafaelvaloto/Pico_W-Dualsense) — Original inspiration
- [egormanga/SAxense](https://apps.sdore.me/SAxense) & [Nielk1's research](https://github.com/egormanga/SAxense/issues/1) — Bluetooth Haptics POC & packet analysis
- [Sony DualSense Wiki](https://controllers.fandom.com/wiki/Sony_DualSense) — DualSense HID report structure
- [Paliverse/DualSenseX](https://github.com/Paliverse/DualSenseX) — Speaker report packets

---

## Credits & License

- **Original Project & Upstream Author:** [awalol](https://github.com/awalol) and all contributors to [awalol/DS5Dongle](https://github.com/awalol/DS5Dongle).
- **This Fork:** Maintained independently by **TochaRude** with the assistance of **Antigravity** (no affiliation or cooperation with the original author).
- **License:** Distributed under the [MIT License](./LICENSE).
