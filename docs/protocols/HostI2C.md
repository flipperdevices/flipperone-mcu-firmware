# Host I²C interface

The Linux CPU controls the MCU through a register file on I²C: it reads input and status,
and writes the CPU state, LEDs, backlight, haptics and firmware updates.

## Counterpart

- The Linux MCU driver, through regmap.
- The updater registers: [flipperos-installer](https://github.com/flipperdevices/flipperos-installer),
  see [FirmwareUpdate.md](FirmwareUpdate.md).

## Transport

- The MCU is an I²C target at address **0x69**.
- **Write:** a transaction starts with the 16-bit register address, high byte first, then
  the data. Data bytes go to consecutive addresses. Every write transaction starts with an
  address; it never continues an earlier one.
- **Read:** write the two address bytes, then read, after a repeated START or in a later
  transaction. Reads auto-increment the address.
- **Values:** registers are 16 bits wide, low byte first. The MCU acts on a register write
  when its high byte arrives, so write both bytes in one transaction.
- **Interrupt line:** the MCU signals a pending, unmasked interrupt on the `CPU_INT` line
  (see the status register below).

## Source of truth

[`applications/services/i2c_intercom/i2c_registers_map.h`](../../applications/services/i2c_intercom/i2c_registers_map.h):
every register, its access and its bits. Wire values shared with Linux:
`CpuState` and `HostResetResponse` in [`host.h`](../../applications/services/host/host.h),
and the updater enums in [`updater.h`](../../applications/services/updater/updater.h).

## Versioning

The intercom version register (`0x0080`, read-only) holds `I2C_INTERCOM_VERSION`,
currently `0x0001`. Bump it with any change a driver written for the previous version
would misread: a register that moves, changes meaning or changes width. Adding a register
or a bit in a reserved field does not need a bump.

## Register blocks

| Base | Block | Access | Notes |
| --- | --- | --- | --- |
| `0x0000` | Status | read | Which interrupt groups are pending |
| `0x0040` | CPU state | read, write | Written by Linux: [HostLifecycle.md](HostLifecycle.md) |
| `0x0080` | Intercom version | read | See above |
| `0x0090` | System reset mode and delay | read, write | The MCU's response to the host going down |
| `0x0100`, `0x0180` | Input interrupts and mask | read to clear; read, write | Buttons, touchpad, headphones, software buttons |
| `0x0104`, `0x0184` | System interrupts and mask | read to clear; read, write | MCU restart, frame request |
| `0x0200` | Input state | read | Buttons, touchpad, headphones, software buttons |
| `0x0300`, `0x0310` | LED brightness and colours | read, write | Colours are RGB565 |
| `0x0350` | Backlight | read, write | Level, timeout, control |
| `0x0400` | Haptics | write | Effect, duration, play or stop |
| `0xF000` | Updater | mixed | [FirmwareUpdate.md](FirmwareUpdate.md) |

## Interrupts

1. An event sets a bit in an interrupt register (`0x0100` input, `0x0104` system), and the
   status register (`0x0000`) shows which group is pending.
2. While any unmasked interrupt is pending, the MCU asserts `CPU_INT`.
3. Linux reads the interrupt register, which clears it, then reads the state registers.

A mask bit set to 1 masks that interrupt: its events are not recorded at all, so unmasking
it later does not raise the ones that happened meanwhile.

## Settings across an MCU restart

The MCU keeps some settings in flash: LED brightness and the backlight level. Everything
else Linux wrote is lost when the MCU restarts: the CPU state, the interrupt masks, the LED
colours. The MCU then raises the **restart** system interrupt (bit 0), and Linux writes
those settings again.

The **frame request** system interrupt (bit 1) asks Linux to send its current frame: the
MCU shows the Linux screen again but has no frame for it ([Display.md](Display.md)).

## Changing it

- Edit `i2c_registers_map.h` first; this document describes the model, not each bit.
- Change the Linux MCU driver in step, and bump the intercom version when the change is
  incompatible.
- Updater registers: also `scripts/updater.py` and flipperos-installer.
