"""Writing a firmware image into a slot: preflight, verified write, TBYB activation."""

import hashlib
import os
import tempfile
import time
from contextlib import contextmanager

from .. import constants
from ..packets import openocd_output, picobin, uf2
from . import swd
from .device import DeviceSession

# The image buys itself after the vector is armed; this bounds the wait, in seconds.
STAGE_TIMEOUT = 120.0
# The image definition sits at byte 312 of current builds, so read a whole sector.
HEADER_WORDS = constants.SECTOR // 4
# picotool's boot-info block, the only section a UF2 may have outside the slot.
BOOT_INFO_ADDRESS = 0x10FFFF00


def preflight(slot, path):
    """Check the image against the slot; returns what the write needs."""
    slot = slot.upper()
    offset, size = swd.partition_bounds(slot)
    address = constants.XIP_BASE + offset
    with open(path, "rb") as source:
        data = source.read()
    plan = {"slot": slot, "offset": offset, "address": address, "size": size, "sha256": hashlib.sha256(data).hexdigest(), "file": os.path.abspath(path)}
    if path.endswith(".uf2"):
        sections = uf2.parse_sections(data)
        inside = [section for section in sections if any(address <= block[0] < address + size for block in section["blocks"].values())]
        for section in sections:
            if section in inside:
                continue
            blocks = list(section["blocks"].values())
            if len(blocks) == 1 and blocks[0][0] == BOOT_INFO_ADDRESS:
                continue
            start = min(block[0] for block in blocks)
            raise ValueError(f"UF2 section at 0x{start:08x} is outside slot {slot} (0x{address:08x}..0x{address + size:08x}); only the boot-info block at 0x{BOOT_INFO_ADDRESS:08x} may sit outside a slot")
        if not inside:
            found = [block for section in sections for block in section["blocks"].values()]
            low = min(start for start, _ in found)
            high = max(start + len(payload) for start, payload in found)
            raise ValueError(f"UF2 data at 0x{low:08x}..0x{high:08x} is outside slot {slot} (0x{address:08x}..0x{address + size:08x})")
        for section in inside:
            if len(section["blocks"]) != section["total"]:
                first = min(start for start, _ in section["blocks"].values())
                raise ValueError(f"UF2 section at 0x{first:08x} is truncated: {len(section['blocks'])} of {section['total']} blocks, the image would be written incomplete")
        image_address, payload = uf2.contiguous(sorted(block for section in inside for block in section["blocks"].values()))
        if not (address <= image_address and image_address + len(payload) <= address + size):
            raise ValueError(f"UF2 data at 0x{image_address:08x}..0x{image_address + len(payload):08x} is outside slot {slot} (0x{address:08x}..0x{address + size:08x})")
        plan.update(kind="uf2", payload=payload, payload_address=image_address, image=picobin.parse_image_def(payload[: constants.SECTOR]))
    elif path.endswith(".bin"):
        if len(data) > size:
            raise ValueError(f"{path} is {len(data)} bytes, larger than slot {slot} ({size} bytes)")
        plan.update(kind="bin", payload=data, payload_address=address, image=picobin.parse_image_def(data))
    else:
        raise ValueError(f"'{path}' is neither a .bin nor a .uf2 file")
    return plan


def slot_image(plan):
    """The picobin image definition now in the slot, read over SWD."""
    words = swd.read_words([(constants.RAW_FLASH_BASE + plan["offset"], HEADER_WORDS)])[0]
    return picobin.parse_image_def(b"".join(word.to_bytes(4, "little") for word in words))


def wait_for_buy(plan, timeout=STAGE_TIMEOUT):
    """Wait for the bootrom to buy the staged image; returns its new image definition."""
    deadline = time.monotonic() + timeout
    while True:
        try:
            image = slot_image(plan)
            if image.get("state") == "valid" and not image.get("tbyb_pending"):
                return image
        except (TimeoutError, OSError):
            pass  # the bootrom rewrites the sector mid-boot
        except RuntimeError as error:
            # Probe and OpenOCD failures are not transient.
            if openocd_output.failure_hint(str(error)) or "OpenOCD not found" in str(error):
                raise
        if time.monotonic() >= deadline:
            raise RuntimeError(
                f"Slot {plan['slot']} still holds an unbought image after {timeout:.0f}s: the staged boot did not run. "
                "Reset the MCU ('f1.py swd reset') and try again, and check 'f1.py log' for the bootrom's verdict"
            )
        time.sleep(2.0)


def write(plan, activate=False):
    """Write a preflight plan, verify it by readback and reset; optionally stage a TBYB buy boot."""
    tbyb = bool(plan["image"].get("tbyb_pending"))
    temp = None
    try:
        # Write the bytes preflight() measured, not the file it read.
        temp = tempfile.NamedTemporaryFile("wb", suffix=".bin", delete=False)
        temp.write(plan["payload"])
        temp.close()
        quoted = "{" + temp.name + "}"
        # reset halt, not halt: after a TBYB revert the core sits in the bootrom,
        # where the flash driver fails with "Failed to invoke rcp_init".
        commands = ["reset halt", f"flash write_image erase {quoted} {plan['payload_address']:#x} bin", f"verify_image {quoted} {plan['payload_address']:#x} bin"]
        staged = activate and tbyb
        if staged:
            # The armed vector turns the reset into a flash-update boot; the running
            # image buys itself through the updater. The scratch survives on B3.
            commands += [f"f1_stage_tbyb {plan['address']:#x}", "reset run"]
        else:
            commands.append("reset run")
        swd.run(commands, timeout=300)
        if staged:
            wait_for_buy(plan)
    finally:
        if temp is not None:
            os.unlink(temp.name)
    plan.update(staged=staged, tbyb=tbyb)
    return plan


def flash(slot, path, activate=False):
    """preflight() and write() in one step, for callers that do not need the plan first."""
    return write(preflight(slot, path), activate=activate)


@contextmanager
def freeze_host():
    """Hold the host CPU in reset through the probe while the write runs."""
    with DeviceSession("probe") as probe:
        pending = None
        try:
            # Release in the finally: the probe may apply the reset without acking it.
            probe.command("gpio cpu_reset 0", 3.0)
            yield
        except BaseException as error:
            pending = error
            raise
        finally:
            try:
                probe.command("gpio cpu_reset 1", 3.0)
            except (RuntimeError, TimeoutError, OSError) as release_error:
                if pending is None:
                    raise  # nothing else failed
                # The host may still be held: say so, without hiding the write failure.
                raise RuntimeError(f"{pending}\nwarning: releasing cpu_reset also failed: {release_error}") from pending
