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

## Join development

* Check the public task tracker: [MCU Firmware Project](https://github.com/orgs/flipperdevices/projects/8)

* Read the documentation: [docs.flipper.net/one/tech-specs](https://docs.flipper.net/one/tech-specs)
  ⚠️ *Co-processor architecture documentation is coming soon (TODO).*

## How to build

### Command line (Ubuntu 24.04+, macOS)

Install the host tools once:

```shell
# Ubuntu
sudo apt install build-essential cmake ninja-build git curl python3-venv

# macOS
xcode-select --install
brew install cmake ninja
```

Then clone and build:

```shell
git clone --recursive https://github.com/flipperdevices/flipperone-mcu-firmware.git
cd flipperone-mcu-firmware
make
```

The first run fetches the ARM GCC toolchain and the Pico SDK — the versions CI builds with, about 1.5 GB unpacked — into `~/.cache/flipperone-mcu-firmware` (change it with `TOOLCHAIN_DIR=...`). If the VS Code extension has already installed them under `~/.pico-sdk`, those are used and nothing is downloaded. To build with your own copies: `make PICO_SDK_PATH=... PICO_TOOLCHAIN_PATH=...`.

The firmware is `build/flipperone-mcu-firmware.uf2`, the partition table `build/partition_table.uf2`.

| Command | Effect |
| --- | --- |
| `make` | Configure on first run, then build |
| `make FW_TARGET=f2` | Reconfigure for another board, see [Hardware targets](#hardware-targets) |
| `make flash` | Write the partition table and firmware over SWD with openocd (CMSIS-DAP probe) |
| `make load` | Write the firmware over USB with picotool (board in BOOTSEL mode) |
| `make clean`, `make distclean` | Remove the build outputs, or the whole build directory |
| `make help` | All targets and the paths in use |

`make flash` needs an openocd that knows the RP2350: [Raspberry Pi's build](https://github.com/raspberrypi/openocd), or the one installed by the VS Code extension, which is picked up automatically. `make load` needs [picotool](https://github.com/raspberrypi/picotool) (`brew install picotool` on macOS).

### VS Code

Install [VSCode](https://code.visualstudio.com/) with the [Raspberry Pi Pico extension](https://marketplace.visualstudio.com/items?itemName=raspberry-pi.raspberry-pi-pico). The extension automatically downloads the ARM toolchain, CMake, Ninja, and Pico SDK. Open the project folder, copy `vscode_template` folder contents to `.vscode` and use the extension's compile button.

On Linux and macOS the VS Code tasks run `make`, so both ways resolve the SDK and toolchain the same way and share the `build/` directory.

### Hardware targets

Each board is described by `targets/<name>/target.cmake`, and a target can be based on another one. `make` builds `f2` unless told otherwise; CMake on its own, and so VS Code, defaults to `f1`. To build a different board, run `make FW_TARGET=<name>`, the **Select Target** task in VSCode (`Terminal → Run Task`), or pass `-DFW_TARGET=<name>` to CMake.

See [TARGETS.md](TARGETS.md) for the descriptor API, how inheritance works, and how to add a board.

## How to update MCU firmware

The firmware uses the [UF2](https://github.com/microsoft/uf2) format for the RP2350 microcontroller.

### On a Flipper One device

The procedure for entering firmware update mode on the real device differs from a bare board. Follow the official guide: [docs.flipper.net/one/mcu-firmware/firmware-update](https://docs.flipper.net/one/mcu-firmware/firmware-update).

### On a plain RP2350 board

1. Enter bootloader mode: hold the **BOOTSEL** button on the RP2350 while connecting USB
2. A USB mass storage device will appear on your computer
3. Copy the `.uf2` firmware file to the mass storage device
4. The device will automatically reboot with the new firmware

Alternatively, you can flash the firmware with [`picotool`](https://github.com/raspberrypi/picotool) (it is downloaded automatically during the CMake configuration step):

```shell
picotool load -x build/flipperone-mcu-firmware.uf2
```
