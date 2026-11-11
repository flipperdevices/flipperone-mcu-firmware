"""Full update-switch E2E, step 2: verify the new image booted and committed.

Asserts the expected partition is running via the flash-update path with the
accepted version, then re-checks after the buy watchdog window (a failed buy
rolls back within ~17 s).

Usage: sudo python3 updater_e2e_verify.py [settle_seconds]
"""

import json
import sys
import time
from pathlib import Path

sys.path.insert(0, "/home/user")
from updater import McuI2C, Reg, get_boot_status, get_state_valid, UpdateError

STATE_PATH = Path("/home/user/updater_e2e_state.json")
BUY_WATCHDOG_S = 17


def read_boot(mcu):
    for _ in range(25):
        partition, boot_type = get_boot_status(mcu)
        if partition in ("A", "B"):
            return partition, boot_type
        time.sleep(0.2)
    return None, None


def main() -> int:
    if not STATE_PATH.exists():
        print(f"Missing {STATE_PATH}; run updater_e2e.py first")
        return 1
    state = json.loads(STATE_PATH.read_text())
    expected_partition = state["expected_partition"]

    with McuI2C() as mcu:
        try:
            get_state_valid(mcu)
        except UpdateError:
            print("MCU not responding")
            return 1

        partition, boot_type = read_boot(mcu)
        fw_version = mcu.read_register(Reg.FW_VERSION)
        print(f"Booted: partition={partition} boot_type={boot_type} version=0x{fw_version:04X}")

        problems = []
        if partition != expected_partition:
            problems.append(f"partition={partition}, expected={expected_partition} (old image still active)")
        if boot_type != 1:
            problems.append(f"boot_type={boot_type}, expected flash-update (1)")
        if fw_version != state["expected_version"]:
            problems.append(f"version=0x{fw_version:04X}, expected=0x{state['expected_version']:04X}")
        if problems:
            print("VERIFY FAILED: " + "; ".join(problems))
            return 1

        # A failed buy rolls back within the bootrom watchdog: re-check after it
        settle = int(sys.argv[1]) if len(sys.argv) > 1 else BUY_WATCHDOG_S
        time.sleep(settle)
        partition, boot_type = read_boot(mcu)
        if partition != expected_partition or boot_type != 1:
            print(f"Rolled back after {settle}s: partition={partition} boot_type={boot_type}")
            return 1

    print("VERIFY PASSED: new image running and committed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
