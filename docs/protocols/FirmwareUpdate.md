# Firmware update from Linux

Linux writes a new MCU firmware image over I²C into the inactive slot, then arms the MCU
to switch to it when Linux powers off or reboots.

## Counterpart

- [flipperos-installer](https://github.com/flipperdevices/flipperos-installer), the
  updater on the Linux side.
- [`scripts/updater.py`](../../scripts/updater.py), the reference implementation, also
  usable by hand on the device.

## Transport

The updater registers of the [host I²C interface](HostI2C.md), at `0xF000`, plus the
system reset registers at `0x0090`.

## Source of truth

- Registers: [`i2c_registers_map.h`](../../applications/services/i2c_intercom/i2c_registers_map.h),
  section *Updater registers*.
- States, errors, commands and boot types: [`updater.h`](../../applications/services/updater/updater.h).
  `scripts/updater.py` mirrors them.

## Versioning

The intercom version register covers this protocol too ([HostI2C.md](HostI2C.md#versioning)).

Firmware versions are reported as two registers: `major << 8 | minor`, and the patch.

## The image

- The firmware `.bin`: `build/<preset>/flipperone-mcu-firmware.bin`, or CI's
  `flipper-one-mcu-<target>-firmware-<suffix>.bin`. It runs from either slot.
- Padded with zeros to whole 256-byte blocks.
- At least 64 blocks (16 KiB), at most the size of a slot.
- Checksums are CRC-16/XMODEM (polynomial `0x1021`, initial value 0; Python's
  `binascii.crc_hqx(data, 0)`).
- A lower version than the running one is accepted.

## Registers

| Address | Register | Access |
| --- | --- | --- |
| `0xF000` | Status: low byte state, high byte error | read |
| `0xF002` | Running firmware version, `major << 8 \| minor` | read |
| `0xF004` | Command: 1 start, 2 abort | write |
| `0xF006` | Boot status: low byte active slot (`'A'` or `'B'`), high byte boot type | read |
| `0xF008`, `0xF00C` | Version and patch of the last accepted update; 0 before the first | read |
| `0xF00A` | Running firmware patch | read |
| `0xF020` | CRC of the whole padded image | write |
| `0xF022` | Number of blocks | write |
| `0xF040` | Block data: 256 bytes, block index, block CRC | write |

## Sequence

1. **Idle.** Read the status. If it is not *Idle* (0), send *abort* and wait for *Idle*.
2. **Parameters.** Write the image CRC (`0xF020`) and the block count (`0xF022`). Both are
   accepted only while *Idle*.
3. **Start.** Write command 1. The state goes *Busy* (2) while the inactive slot is erased,
   then *Running* (1). Erasing can take up to two minutes.
4. **Blocks.** For each block, from 0 up and in order: wait for *Running*, then write the
   whole frame to `0xF040` in one transaction: 256 data bytes, the block index and the
   block's CRC, both 16-bit little-endian. The state is *Busy* while the MCU writes the
   block.
5. **Done.** After the last block the MCU checks the image CRC and stages the image: the
   state becomes *Done* (3). Allow a few seconds.
6. **Arm.** Write the delay (`0x0094`, in 100 ms units), then the system reset mode
   (`0x0092`) = 1. When Linux reports *powered off* or *restarting*
   ([HostLifecycle.md](HostLifecycle.md)), the MCU cuts host power and boots the staged
   image. After *restarting* the new firmware powers the host again; after *powered off*
   it leaves it off. If Linux reports a running state before the delay has passed, the
   switch is cancelled.

The MCU forgets the armed mode when it resets. A new *start* or an *abort* discards the
staged image.

## After the switch

The new image runs once through the bootrom's flash-update boot and accepts itself
(try-before-you-buy). The boot type in the boot status register then reads:

| Boot type | Meaning |
| --- | --- |
| 0 | Normal boot, not after an update |
| 1 | Switched to the new image, which accepted itself |
| 2 | Reverted: the new image failed before accepting itself, and the old one runs |

## Errors

On an error the state is *Error* (4) and the high byte of the status holds the code. Send
*abort* to return to *Idle*, then start again.

| Code | Error | Cause |
| --- | --- | --- |
| 1 | Not started | Updates are not possible: no inactive slot |
| 2 | Size | Too few blocks, or more than a slot holds |
| 3 | Block CRC | A block's CRC does not match its data |
| 4 | Block index | A block arrived out of order |
| 5 | Image CRC | The whole image's CRC does not match |
| 6 | Version | The image carries no version |
| 7 | Protocol | Malformed or out-of-order I²C data |
| 8 | Write | Writing flash failed |
| 9 | Invalidate | Reserved |
| 10 | Parameters | *Start* without the CRC or block count |

## Changing it

- `updater.h` and `i2c_registers_map.h` define the protocol; `scripts/updater.py` and
  flipperos-installer change with them.
- Keep the values of states, errors and commands stable: the Linux side depends on them.
