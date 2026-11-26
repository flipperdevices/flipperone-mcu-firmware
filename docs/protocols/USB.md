# USB

How the MCU appears on USB.

> [!NOTE]
> Not written yet. The sections follow [the contract template](README.md#writing-a-contract-document).

- **Counterpart:** the host OS, picotool and the agent tools.
- **Source of truth:** [`lib/tusb/usb_descriptors.c`](../../lib/tusb/usb_descriptors.c); the BOOTSEL white label in [../OTP.md](../OTP.md).

Planned contents:

- VID, PID and strings
- The interfaces: the CDC ports and what each carries
- How a device is told apart: serial numbers
