# Flipper One MCU Firmware

[![MemBrowse](https://membrowse.com/badge.svg)](https://membrowse.com/public/flipperdevices/flipperone-mcu-firmware)

Firmware for the Flipper One low-power MCU (Raspberry Pi RP2350): the co-processor that runs the display, buttons, touchpad, LEDs, battery and power, next to the Linux CPU. How the two work together: [docs/Architecture.md](docs/Architecture.md).

## Get the firmware

[**📥 Latest dev build**](https://update.flipperzero.one/builds/flipper-one-mcu/dev/). CI builds every push to `dev`, every tag and every pull request; a bot comment links a pull request's build.

- On a Flipper One: follow the [firmware update guide](https://docs.flipper.net/one/mcu-firmware/firmware-update).
- On a bare RP2350 board, and to recover a device that no longer boots: [docs/Flashing.md](docs/Flashing.md).

## Build

```shell
git clone --recursive https://github.com/flipperdevices/flipperone-mcu-firmware.git
cd flipperone-mcu-firmware

source scripts/toolchain/fbtenv.sh     # PowerShell: . .\scripts\toolchain\fbtenv.ps1
cmake --preset f2-release
cmake --build --preset f2-release
```

The firmware is `build/f2-release/flipperone-mcu-firmware.uf2`. The first run downloads the pinned toolchain (400–500 MB). Other boards, host requirements and other setups: [docs/Building.md](docs/Building.md). Editor setup: [docs/VSCode.md](docs/VSCode.md).

## Documentation

| Document | Covers |
| --- | --- |
| [Architecture](docs/Architecture.md) | The two processors and how the firmware is organised |
| [Building](docs/Building.md) | The toolchain, presets, the toolchain check |
| [VSCode](docs/VSCode.md) | clangd, tasks, debug configurations |
| [Flashing](docs/Flashing.md) | Devices, bare boards, slots, recovery |
| [Debugging](docs/Debugging.md) | Logs, the debugger, crash reports |
| [Testing](docs/Testing.md) | On-device unit tests and host tests |
| [Protocols](docs/protocols/README.md) | Contracts with Linux and host tools |
| [All documents](docs/README.md) | Targets, OTP, applications and the rest |

## Contributing

Read [CONTRIBUTING.md](CONTRIBUTING.md) and pick a task from the [MCU Firmware project](https://github.com/orgs/flipperdevices/projects/8). Product specifications: [docs.flipper.net/one/tech-specs](https://docs.flipper.net/one/tech-specs).

## License

Mostly MIT; some files ported from the Flipper Zero firmware are GPL-3.0-or-later, and assets and documentation are CC-BY-SA-4.0. Details: [LICENSE.md](LICENSE.md).
