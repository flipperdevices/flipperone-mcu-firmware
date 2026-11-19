"""Values shared across layers: USB identities, roles, bauds, paths."""

from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
PARTITION_TABLE = REPO_ROOT / "targets" / "partition_table.json"

PROBE_VID, PROBE_PID = 0x37C1, 0xF131
PROBE_SERIAL = "f1_debug"  # a real serial is f1_debug_<16 bits of the chip ID>_
PROBE_SERIAL_PATTERN = r"f1_debug_[0-9a-f]{4}_"
OLD_PROBE = "its firmware is older than 0.7.0, which these tools do not support: update the probe"
MCU_VID, MCU_PID, MCU_SERIAL = 0x37C1, 0xF101, "flip_one_mcu"

# role: (USB serial number, CDC control interface, baud rate, description)
ROLES = {
    "linux": (PROBE_SERIAL, 1, 1500000, "Linux console (via probe UART)"),
    "mcu": (PROBE_SERIAL, 3, 1500000, "MCU console (via probe UART)"),
    "mcu-log": (PROBE_SERIAL, 5, 1500000, "MCU log, output only"),
    "probe": (PROBE_SERIAL, 7, 115200, "Debug probe console"),
    "mcu-usb": (MCU_SERIAL, 0, 115200, "MCU console (MCU's own USB, only while Linux is off)"),
}

CPU_APP = "cpu_app_start"  # the MCU app that powers the host and shows its screen

XIP_BASE = 0x10000000
RAW_FLASH_BASE = 0x1C000000  # flash without address translation
QMI_ATRANS0 = 0x400D0034
TIMER0_TIMERAWL = 0x400B0028
DHCSR = 0xE000EDF0
SECTOR = 0x1000
FIRST_PARTITION_OFFSET = 0x2000
