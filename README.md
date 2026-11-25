# Flipper One MCU Firmware

[![MemBrowse](https://membrowse.com/badge.svg)](https://membrowse.com/public/flipperdevices/flipperone-mcu-firmware)

This repository is part of [Flipper One MCU Firmware](https://github.com/orgs/flipperdevices/projects/8) sub-project and contains issue tracking and firmware sources for the Flipper One MCU — the low-power co-processor that controls the LCD, buttons, and battery.

<img width="1474" height="450" alt="Flipper One MCU and CPI interconnection" src="https://github.com/user-attachments/assets/67d4810f-38b0-49af-8321-11bbc84ed04d" />

### Flipper One uses a dual-processor architecture:

* **Low-Power MCU** (Raspberry Pi RP2350)  
  Buttons, LCD display, touchpad, and LEDs are physically connected to the MCU. It also manages battery and power control.  
  To render graphics on the LCD from Linux, the main CPU transfers display data to the MCU over SPI.  
  When the device is powered off, the MCU controls power-bank mode and system power states.  
  The MCU also participates in booting the main CPU.

* **High-Performance Linux CPU** (Rockchip RK3576)  
  This processor runs Linux, and all high-level peripherals are connected to it: USB, HDMI, M.2, Wi-Fi, Ethernet, and audio.

The MCU and CPU are interconnected via several interfaces: SPI, I²C, and UART. Additional GPIO lines are used for BOOT_0, BOOT_1, and IRQ signals.

## Automated builds
Builds run automatically on every push to the `dev` branch, on tag pushes, and on pull requests. PR builds are linked from a bot comment on the pull request.

### [`📥 Download latest dev firmware →`](https://update.flipperzero.one/builds/flipper-one-mcu/dev/)
**⚠️ TODO:** make a proper build server address and folder structure instead of using `flipperzero.one`

## Manual build 

**⚠️ TODO:** how to build manually? 

## Join development

* Check the public task tracker: [MCU Firmware Project](https://github.com/orgs/flipperdevices/projects/8)

* Read the documentation: [docs.flipper.net/one/tech-specs](https://docs.flipper.net/one/tech-specs)
  ⚠️ *Co-processor architecture documentation is coming soon (TODO).*

## How to build

Every tool the build needs comes from one pinned release of [flipperone-mcu-toolchain](https://github.com/flipperdevices/flipperone-mcu-toolchain): Arm GCC, the Pico SDK, picotool, OpenOCD, CMake, Ninja, and Python with the packages the generators use. [`toolchain.json`](toolchain.json) names the release, and a script sets it up:

```shell
git clone --recursive https://github.com/flipperdevices/flipperone-mcu-firmware.git
cd flipperone-mcu-firmware

source scripts/toolchain/fbtenv.sh     # PowerShell: . .\scripts\toolchain\fbtenv.ps1
cmake --preset f2-release
cmake --build --preset f2-release
```

The output firmware file will be at `build/f2-release/flipperone-mcu-firmware.uf2`.

The first time, `fbtenv` downloads the archive for your machine (400–500 MB), checks its sha256 against `toolchain.json`, and unpacks it into `~/.flipperone-mcu-toolchain/<tag>/`. After that it only sets up the environment and points `~/.flipperone-mcu-toolchain/current` at that release (a junction on Windows); run it once in every new shell. To set it up offline, download the archive yourself and point `FLIPPERONE_TOOLCHAIN_ARCHIVE` at it.

The toolchain runs on Linux x86_64 and aarch64 with glibc 2.34 or newer, macOS on Apple silicon, and Windows x64. It brings its own USB libraries; on Linux it needs only `libudev1`, which systemd-based distributions have, plus udev rules to use a debug probe or flash without root (see the toolchain's [README](https://github.com/flipperdevices/flipperone-mcu-toolchain#host-requirements)).

### Hardware targets

Each board is described by `targets/<name>/target.cmake`, and a target can be based on another one. The default is `f2`. Each preset builds in its own `build/<preset>/` directory, so switching boards needs no rebuild:

| Preset | Builds |
| --- | --- |
| `f2-release` | f2 firmware |
| `f2-recovery` | f2 firmware and `recovery.uf2` |
| `f1-release` | f1 firmware |

Without a preset, pass `-DFW_TARGET=<name>` to CMake. See [TARGETS.md](TARGETS.md) for the descriptor API, how inheritance works, and how to add a board.

### VSCode

Copy the `vscode_template` folder contents to `.vscode` and install the recommended extensions (CMake Tools, Cortex-Debug). Set up the toolchain once with `fbtenv` as above; after that VSCode finds it in `~/.flipperone-mcu-toolchain` by itself. The build uses the release `toolchain.json` pins; the debugger, OpenOCD and picotool come from the `current` link, the release `fbtenv` set up last. Pick a preset in the CMake Tools panel, build, and debug with the **Debug** configurations. The **Run Project**, **Write partition table** and **Flash** tasks act on the selected preset's build. The template expects the toolchain in `~/.flipperone-mcu-toolchain`: with `FLIPPERONE_TOOLCHAIN_HOME` set elsewhere, change its paths to match.

### Other setups

Configuring checks the tools against `toolchain.json`: with the toolchain, that it is the pinned release; without it, that the Pico SDK and GCC versions match. A mismatch stops the configuration with a hint, and `-DFW_TOOLCHAIN_CHECK=WARN` builds anyway. Only the toolchain release reproduces CI's binaries exactly.

The [Raspberry Pi Pico extension](https://marketplace.visualstudio.com/items?itemName=raspberry-pi.raspberry-pi-pico) installs its own Pico SDK and Arm GCC, at the versions on the `sdkVersion`, `toolchainVersion` and `picotoolVersion` lines of `CMakeLists.txt`, which match `toolchain.json`. A new build directory uses them only when the pinned toolchain is neither set up with `fbtenv` nor installed; an existing one keeps the SDK it was configured with.

## How to update MCU firmware

The firmware uses the [UF2](https://github.com/microsoft/uf2) format for the RP2350 microcontroller.

### On a Flipper One device

The procedure for entering firmware update mode on the real device differs from a bare board. Follow the official guide: [docs.flipper.net/one/mcu-firmware/firmware-update](https://docs.flipper.net/one/mcu-firmware/firmware-update).

### On a plain RP2350 board

1. Enter bootloader mode: hold the **BOOTSEL** button on the RP2350 while connecting USB
2. A USB mass storage device will appear on your computer
3. Copy the `.uf2` firmware file to the mass storage device
4. The device will automatically reboot with the new firmware

Alternatively, you can flash the firmware with [`picotool`](https://github.com/raspberrypi/picotool), which comes with the toolchain:

```shell
picotool load -x build/f2-release/flipperone-mcu-firmware.uf2
```

### Recovering the MCU

If the MCU no longer boots, flash the recovery image: `flipper-one-mcu-<target>-recovery-<version>.uf2` from CI, or `build/f2-recovery/recovery.uf2` from the `f2-recovery` preset (any build configured with `-DFW_RECOVERY=ON` makes one). It holds the partition table and the firmware.

1. Put the MCU into BOOTSEL (DFU) mode as described in the [firmware update guide](https://docs.flipper.net/one/mcu-firmware/firmware-update)
2. Copy the recovery file onto the drive that appears
