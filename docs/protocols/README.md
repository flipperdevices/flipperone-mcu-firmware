# Protocols and contracts

The interfaces where the firmware meets code it does not own: the Linux side, host tools and scripts. Each document describes one contract.

| Document | Contract | Counterpart |
| --- | --- | --- |
| [HostI2C](HostI2C.md) | The I²C register interface | Linux MCU driver |
| [FirmwareUpdate](FirmwareUpdate.md) | Updating the MCU firmware from Linux | [flipperos-installer](https://github.com/flipperdevices/flipperos-installer) |
| [HostLifecycle](HostLifecycle.md) | CPU states, host power, BOOT and IRQ lines | Linux MCU driver |
| [Display](Display.md) | Frames from Linux over SPI, screen handover | Linux display driver |
| [RPC](RPC.md) | Protobuf RPC over the console | Host tools |
| [Console](Console.md) | Serial ports and the CLI output scripts parse | `scripts/testops.py`, agent tools |
| [USB](USB.md) | USB identity and interfaces | Host OS, tools |

The data the firmware keeps in OTP is in [../OTP.md](../OTP.md).

## Writing a contract document

Every document has these sections, in this order:

1. **Counterpart:** who is on the other side, and where that code lives.
2. **Transport:** the bus or channel, and the framing on it.
3. **Source of truth:** the header or `.proto` that defines the details. The document explains the model and links the file; it does not copy tables the file owns.
4. **Versioning:** how each side tells which version the other speaks, and what stays compatible.
5. **Contract:** messages or registers, their meaning, ordering, timing and errors.
6. **Changing it:** what has to change together, in this repository and elsewhere.

## Changing a contract

A pull request that changes a contract updates its document, and its version where it has one, in the same pull request. The counterpart is changed in step with it; say in the pull request how.
