# Flipper One MCU Firmware

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

### Quick setup (recommended)

Install the [Raspberry Pi Pico VS Code Extension](https://marketplace.visualstudio.com/items?itemName=raspberry-pi.raspberry-pi-pico). It installs CMake, the ARM GCC toolchain, the Pico SDK, and picotool under `~/.pico-sdk/` and is picked up automatically by CMake. No further setup required.

### Manual setup

If you prefer not to use VS Code, install the following manually:

- [CMake](https://cmake.org/) >= 3.13
- ARM GCC toolchain (arm-none-eabi), version 14.2 recommended  
  Download: [Arm GNU Toolchain Downloads](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)
- [Pico SDK](https://github.com/raspberrypi/pico-sdk) 2.2.0  
  Set the `PICO_SDK_PATH` environment variable to the SDK root before building.
- [picotool](https://github.com/raspberrypi/picotool) 2.2.0 (required for flashing)  
  See the [picotool README](https://github.com/raspberrypi/picotool#building-picotool) for build and install instructions.

### Build steps

```bash
git clone --recursive https://github.com/flipperdevices/flipperone-mcu-firmware
cd flipperone-mcu-firmware

mkdir build && cd build
cmake .. -DFIRMWARE_TARGET=100
cmake --build . --parallel
```

The build produces `flipperone-mcu-firmware.uf2` in the `build/` directory.

## Flashing

### Option 1: UF2 drag-and-drop

1. Hold the BOOTSEL button on the MCU board and connect it via USB.
2. It mounts as a USB mass storage device (`RP2350`).
3. Copy `flipperone-mcu-firmware.uf2` to the drive.
4. The device reboots automatically when the copy finishes.

### Option 2: picotool

```bash
picotool load build/flipperone-mcu-firmware.uf2 --force
picotool reboot
```

### Option 3: Pre-built firmware

Download the latest firmware from [Automated builds](#automated-builds) and flash using either method above.

## Join development

* Check the public task tracker: [MCU Firmware Project](https://github.com/orgs/flipperdevices/projects/8)

* Read the documentation: [docs.flipper.net/one/tech-specs](https://docs.flipper.net/one/tech-specs)  
  ⚠️ *Co-processor architecture documentation is coming soon (TODO).*
