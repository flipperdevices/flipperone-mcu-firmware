# Flashing

The firmware ships as [UF2](https://github.com/microsoft/uf2) files: CI publishes `flipper-one-mcu-<target>-firmware-<suffix>.uf2`, a local build makes `build/<preset>/flipperone-mcu-firmware.uf2`.

## On a Flipper One

Entering firmware update mode on the device differs from a bare board. Follow the official guide: [docs.flipper.net/one/mcu-firmware/firmware-update](https://docs.flipper.net/one/mcu-firmware/firmware-update).

## On a bare RP2350 board

1. Hold **BOOTSEL** while connecting USB. A drive named `RP2350` appears.
2. Copy the `.uf2` onto it. The board reboots into the new firmware.

Or with picotool, which comes with the toolchain:

```shell
picotool load -x build/f2-release/flipperone-mcu-firmware.uf2
```

## Over SWD

With a debug probe, the VSCode **Flash** task writes the partition table and the firmware ([VSCode.md](VSCode.md#build-flash-and-debug)). The agent tools do the same from the command line, with slot selection and checks; see [AGENT_TOOLS.md](../scripts/agent_tools/AGENT_TOOLS.md#firmware-slots-and-swd).

## Slots

- Flash holds a partition table and two firmware slots, A and B ([`targets/partition_table.json`](../targets/partition_table.json)). The bootrom boots the valid image with the higher version; with equal versions, A.
- Builds are try-before-you-buy (TBYB) images: the bootrom runs a newly written image once, through a flash-update boot, and keeps it only after the firmware accepts it. A normal reset skips an image that was never accepted.

## Recovery

If the MCU no longer boots, flash the recovery image. It holds the partition table and the firmware in one UF2, without TBYB:

- from CI: `flipper-one-mcu-<target>-recovery-<suffix>.uf2`
- locally: `build/f2-recovery/recovery.uf2` from the `f2-recovery` preset, or any build configured with `-DFW_RECOVERY=ON`

1. Put the MCU into BOOTSEL mode as the [firmware update guide](https://docs.flipper.net/one/mcu-firmware/firmware-update) describes.
2. Copy the recovery file onto the drive that appears.
