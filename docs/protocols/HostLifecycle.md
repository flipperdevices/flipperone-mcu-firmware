# Host lifecycle

How Linux reports its state to the MCU, and how the MCU powers, restarts and boots the Linux CPU.

> [!NOTE]
> Not written yet. The sections follow [the contract template](README.md#writing-a-contract-document).

- **Counterpart:** the Linux MCU driver.
- **Source of truth:** `CpuState`, `HostResetResponse` and `HostState` in [`host.h`](../../applications/services/host/host.h); the CPU state and system reset registers in [`i2c_registers_map.h`](../../applications/services/i2c_intercom/i2c_registers_map.h).

Planned contents:

- The CPU states, who writes each one, and when
- Power on, off and restart; what the MCU does when Linux stops responding
- The system reset mode and delay
- The BOOT_0 and BOOT_1 lines, and booting into maskrom
- The interrupt line
