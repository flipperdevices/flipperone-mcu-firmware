#!/usr/bin/env python3
"""Flash a firmware build over SWD with a CMSIS-DAP debug probe, and start it.

    python3 scripts/flash.py                     # build/current: partition table, then slot A
    python3 scripts/flash.py --build-dir build/f1-release --slot b

If the other slot holds an image the bootrom would boot instead (a higher version,
or slot A with the same version), its header is erased first. Builds are
try-before-you-buy (TBYB) images, which the bootrom skips on a normal reset. After
writing, this boots the slot once as a flash update, so the firmware accepts the
image, and waits until the bootrom has committed it. Uses the same code as
`scripts/agent_tools/f1.py swd flash --activate`.
"""

import argparse
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from agent_tools import constants  # noqa: E402
from agent_tools.logic import flash, swd  # noqa: E402

FIRMWARE = "flipperone-mcu-firmware.bin"
PARTITION_TABLE = "partition_table.bin"


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build-dir", default=str(constants.REPO_ROOT / "build" / "current"),
                        help="build directory with %s and %s (default: build/current)" % (FIRMWARE, PARTITION_TABLE))
    parser.add_argument("--slot", choices=("a", "b"), default="a", help="firmware slot (default: a)")
    parser.add_argument("--no-partition-table", action="store_true", help="leave the partition table as it is")
    args = parser.parse_args(argv)

    firmware = os.path.join(args.build_dir, FIRMWARE)
    table = os.path.join(args.build_dir, PARTITION_TABLE)
    try:
        if not os.path.isfile(firmware):
            raise ValueError(f"{firmware} not found: build first")
        plan = flash.preflight(args.slot, firmware)
        if not args.no_partition_table:
            if not os.path.isfile(table):
                raise ValueError(f"{table} not found: build first, or pass --no-partition-table")
            print(f"Writing the partition table ({table})...", flush=True)
            flash.write_partition_table(table)
        other = flash.outranking_slot(plan, swd.status()["slots"])
        if other:
            print(f"Slot {other} holds an image the bootrom would boot instead: erasing its header...", flush=True)
            swd.erase_slot_header(other)
        tbyb = " and starting it once, so the try-before-you-buy image accepts itself" if plan["image"].get("tbyb_pending") else ""
        print(f"Writing slot {plan['slot']} at 0x{plan['address']:08x} ({len(plan['payload'])} bytes){tbyb}...", flush=True)
        flash.write(plan, activate=True)
        state = swd.status()
    except (ValueError, RuntimeError, TimeoutError, OSError, subprocess.TimeoutExpired) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print(f"Done: slot {plan['slot']} written and verified; the MCU runs slot {state['active_slot'] or 'none'}.")
    if state["active_slot"] != plan["slot"]:
        print(f"warning: slot {plan['slot']} is not the active one", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
