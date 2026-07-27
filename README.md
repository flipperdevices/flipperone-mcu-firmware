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

Install [VSCode](https://code.visualstudio.com/) with the [Raspberry Pi Pico extension](https://marketplace.visualstudio.com/items?itemName=raspberry-pi.raspberry-pi-pico). The extension automatically downloads the ARM toolchain, CMake, Ninja, and Pico SDK. Open the project folder and use the extension's compile button.

<details>
<summary>Manual build (Linux / macOS)</summary>

Prerequisites: [ARM GCC toolchain](https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases) (tested with 14.2.1), CMake 3.13+, [Pico SDK 2.2.0](https://github.com/raspberrypi/pico-sdk).

On macOS, the ARM toolchain can be installed via Homebrew:

```shell
brew install --cask gcc-arm-embedded
```

```shell
git clone --recursive https://github.com/flipperdevices/flipperone-mcu-firmware.git
cd flipperone-mcu-firmware

git clone -b 2.2.0 https://github.com/raspberrypi/pico-sdk.git ../pico-sdk
cd ../pico-sdk && git submodule update --init && cd ../flipperone-mcu-firmware

mkdir -p build && cd build
PICO_SDK_PATH=../../pico-sdk cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release --parallel
```

The output firmware file will be at `build/flipperone-mcu-firmware.uf2`.
</details>

<details>
<summary>Manual build (Debian / Ubuntu packages)</summary>

The firmware also builds against distro-packaged tooling, with a picolibc-based ARM toolchain instead of the newlib-based xpack one:

```shell
sudo apt install cmake ninja-build gcc-arm-none-eabi picolibc-arm-none-eabi \
                 pico-sdk-source picotool

git clone --recursive https://github.com/flipperdevices/flipperone-mcu-firmware.git
cd flipperone-mcu-firmware

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DPICO_SDK_PATH=/usr/src/pico-sdk
cmake --build build --parallel
```

Use the distro's own `pico-sdk-source` package here: an upstream Pico SDK checkout builds its
boot stage 2 with `--specs=nosys.specs`, which the Debian toolchain does not ship. Requires
Debian testing or newer (`pico-sdk-source` 2.2.0).
</details>

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
