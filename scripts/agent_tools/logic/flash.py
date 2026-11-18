"""Writing a firmware image into a slot: preflight, verified write, TBYB activation."""

import hashlib
import os
import tempfile
from contextlib import contextmanager

from .. import constants
from ..packets import picobin, uf2
from . import swd
from .device import DeviceSession
# REBOOT2 FLASH_UPDATE vector in the watchdog scratch registers, proven on
# silicon B3 (scripts/i2c_race_issue_notes.md): the bootrom then stages and
# buys a try-before-you-buy image instead of skipping it on a normal boot.
STAGING_VECTOR = [
    "targets rp2350.cm0",
    "mww 0x400d8000 0",  # watchdog control: stop it
    "mww 0x40018008 0x1ffff3",  # POWMAN boot word: clear
    "mww 0x400d8014 {param0}",  # scratch2: update base (XIP address)
    "mww 0x400d8018 0",  # scratch3: unused
    "mww 0x400d801c 0xb007c0d3",  # scratch4: magic
    "mww 0x400d8020 0xfffffffe",  # scratch5: flags
    "mww 0x400d8024 4",  # scratch6: BOOT_TYPE_FLASH_UPDATE
    "mww 0x400d8028 0xb007c0d3",  # scratch7: magic
    "mww 0x40108030 0",  # TIMER watchdog: stop
    "mww 0x40108034 12",  # slow tick, so LOAD spans ~18s inside the bootrom
    "mww 0x40108030 1",
    "mww 0x400d8004 0xffffff",  # watchdog load
    "mww 0x400d8000 0x40000000",  # watchdog trigger: reset into the bootrom
]


def preflight(slot, path):
    """Check the image against the slot; returns what the write needs."""
    slot = slot.upper()
    offset, size = swd.partition_bounds(slot)
    address = constants.XIP_BASE + offset
    with open(path, "rb") as source:
        data = source.read()
    plan = {"slot": slot, "offset": offset, "address": address, "size": size, "sha256": hashlib.sha256(data).hexdigest(), "file": os.path.abspath(path)}
    if path.endswith(".uf2"):
        chunks = uf2.parse_blocks(data)
        image_address, payload = uf2.contiguous(chunks)
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


def flash(slot, path, activate=False):
    """Write the image, verify it by readback and reset; optionally stage a TBYB buy boot."""
    plan = preflight(slot, path)
    tbyb = bool(plan["image"].get("tbyb_pending"))
    commands = ["halt"]
    temp = None
    try:
        if plan["kind"] == "uf2":
            temp = tempfile.NamedTemporaryFile("wb", suffix=".bin", delete=False)
            temp.write(plan["payload"])
            temp.close()
            quoted = "{" + temp.name + "}"
            commands += [f"flash write_image erase {quoted} {plan['payload_address']:#x} bin", f"verify_image {quoted} {plan['payload_address']:#x} bin"]
        else:
            quoted = "{" + plan["file"] + "}"
            commands += [f"flash write_image erase {quoted} {plan['address']:#x} bin", f"verify_image {quoted} {plan['address']:#x} bin"]
        staged = activate and tbyb
        if staged:
            commands += [line.format(param0=f"{plan['address']:#x}") for line in STAGING_VECTOR]
        commands.append("reset run")
        swd.run(commands, timeout=300)
    finally:
        if temp is not None:
            os.unlink(temp.name)
    plan.update(staged=staged, tbyb=tbyb)
    return plan


@contextmanager
def freeze_host():
    """Hold the host CPU in reset through the probe while the write runs."""
    with DeviceSession("probe") as probe:
        probe.command("gpio cpu_reset 0", 3.0)
        try:
            yield
        finally:
            probe.command("gpio cpu_reset 1", 3.0)
