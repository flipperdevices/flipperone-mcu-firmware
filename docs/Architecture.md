# Architecture

> [!NOTE]
> This document is incomplete. Planned: boot and power sequence, threads and their
> priorities, memory and flash layout.

## Two processors

<img width="1474" height="450" alt="Flipper One MCU and CPU interconnection" src="https://github.com/user-attachments/assets/67d4810f-38b0-49af-8321-11bbc84ed04d" />

- **Low-power MCU** (Raspberry Pi RP2350, this firmware). The buttons, LCD, touchpad and
  LEDs are wired to it, and it manages the battery and power. It shows on the LCD what
  Linux sends it over SPI, runs power-bank mode while the device is off, and takes part in
  booting the CPU.
- **Linux CPU** (Rockchip RK3576). Runs Linux and has the high-level peripherals: USB,
  HDMI, M.2, Wi-Fi, Ethernet and audio.

They are connected by SPI, I²C and UART, plus GPIO lines for BOOT_0, BOOT_1 and an IRQ.

## Firmware structure

| Path | Contents |
| --- | --- |
| `applications/applications.c` | The registry of services, apps and CLI commands |
| `applications/services/` | Long-running services: one thread and one event loop each, never torn down |
| `applications/main/`, `applications/debug/` | Apps and CLI commands; hardware tests and the unit tests |
| `lib/` | First-party libraries (`toolbox`, `cli`, `containers`, `drivers`, ...) and submodules |
| `targets/` | Board support, one directory per board: [Targets.md](Targets.md) |
| `assets/` | Images, fonts and protobuf definitions, and their generators |

Writing services and apps: [Applications.md](Applications.md).
