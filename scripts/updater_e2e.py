"""Full update-switch E2E, step 1: update, arm, reboot the host.

Writes the inactive partition and verifies the MCU armed the host-shutdown
reset, then reboots the host. updater_e2e_verify.py (staged to /home/user,
which survives the reboot) checks the switch after the host is back.

Usage: sudo python3 updater_e2e.py <firmware.bin>
"""

import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from updater import McuI2C, Reg, ResetMode, get_boot_status, get_state_valid, load_firmware, run_update, UpdateError

STATE_PATH = Path("/home/user/updater_e2e_state.json")
STAGE_DIR = Path("/home/user")


def wait_until_ready(mcu, timeout=30):
    deadline = time.monotonic() + timeout
    while True:
        try:
            get_state_valid(mcu)
            return True
        except UpdateError:
            if time.monotonic() >= deadline:
                return False
            time.sleep(0.5)


def read_boot(mcu):
    for _ in range(25):
        partition, boot_type = get_boot_status(mcu)
        if partition in ("A", "B"):
            return partition, boot_type
        time.sleep(0.2)
    return None, None


def main() -> int:
    firmware, blocks, _ = load_firmware(Path(sys.argv[1]))

    with McuI2C() as mcu:
        if not wait_until_ready(mcu):
            print("MCU not responding")
            return 1
        partition, boot_type = read_boot(mcu)
        if partition not in ("A", "B") or boot_type != 0:
            print(f"Precondition failed: partition={partition} boot_type={boot_type}")
            return 1
        old_version = mcu.read_register(Reg.FW_VERSION)
        print(f"Current: partition={partition} version=0x{old_version:04X}")

        run_update(mcu, firmware, blocks)

        new_version = mcu.read_register(Reg.LAST_WRITTEN_VERSION)
        mode = 0
        for _ in range(30):
            mode = mcu.read_register(Reg.SYSTEM_RESET_MODE)
            if mode == ResetMode.RESET_MCU:
                break
            time.sleep(0.1)
        print(f"Updated: version=0x{new_version:04X} reset_mode={mode}")
        if new_version == 0 or mode != ResetMode.RESET_MCU:
            print("Updater did not arm the switch; refusing to reboot")
            return 1

        STATE_PATH.write_text(json.dumps({"expected_partition": "B" if partition == "A" else "A", "expected_version": new_version}))
        print(f"State: {STATE_PATH.read_text()}")

    # Stage the verify step somewhere that survives the reboot
    shutil.copy(Path(__file__).with_name("updater.py"), STAGE_DIR / "updater.py")
    shutil.copy(Path(__file__).with_name("updater_e2e_verify.py"), STAGE_DIR / "updater_e2e_verify.py")

    print("Rebooting the host: the MCU must follow via the armed reset")
    subprocess.run(["reboot"], check=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
