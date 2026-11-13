"""SWD access through OpenOCD: memory reads without halting, reset, flashing.

Each call runs one short OpenOCD process. The probe accepts a single client,
so this fails while a debugger (for example a VS Code session) is attached.
"""

import glob
import json
import os
import shutil
import struct
import subprocess

import get_port as ports  # scripts/get_port.py; f1.py puts scripts/ on the path

XIP_BASE = 0x10000000
RAW_FLASH_BASE = 0x1C000000  # flash without address translation
QMI_ATRANS0 = 0x400D0034
TIMER0_TIMERAWL = 0x400B0028
DHCSR = 0xE000EDF0
SECTOR = 0x1000
PICOBIN_BLOCK_MARKER = 0xFFFFDED3
FIRST_PARTITION_OFFSET = 0x2000

MARK = "F1>"
REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))


def find_openocd():
    """Return (binary, scripts dir). Needs a build with RP2350 support, such as the Pico SDK one."""
    override = os.environ.get("F1_OPENOCD")
    candidates = [override] if override else []
    candidates += sorted(glob.glob(os.path.expanduser("~/.pico-sdk/openocd/*/openocd")), reverse=True)
    candidates += sorted(glob.glob(os.path.expanduser("~/.pico-sdk/openocd/*/openocd.exe")), reverse=True)
    for binary in candidates:
        scripts = os.path.join(os.path.dirname(binary), "scripts")
        if os.path.isfile(binary) and os.path.isfile(os.path.join(scripts, "target", "rp2350.cfg")):
            return binary, scripts
    system = shutil.which("openocd")
    if system:
        return system, None
    raise RuntimeError("OpenOCD not found. Install the Pico SDK tooling or set F1_OPENOCD to the binary.")


def run(commands, timeout=120):
    """Run OpenOCD commands after `init`; return the lines printed with `echo "F1> ..."`."""
    binary, scripts = find_openocd()
    body = "; ".join(commands)
    attached = ports.probes()
    select = []
    if len(attached) > 1:
        # Each probe has its own USB serial number, which is what OpenOCD's
        # CMSIS-DAP driver selects by
        select = ["-c", f"adapter serial {ports.selected_probe()['serial']}"]
    script = "; ".join(["adapter speed 5000", "init", body, "shutdown"])
    args = [binary] + (["-s", scripts] if scripts else []) + ["-f", "interface/cmsis-dap.cfg", *select, "-f", "target/rp2350.cfg", "-c", script]
    result = subprocess.run(args, capture_output=True, text=True, timeout=timeout)
    output = result.stdout + result.stderr
    errors = list(dict.fromkeys(line for line in output.splitlines() if line.startswith("Error")))
    if "shutdown command invoked" not in output or errors:
        hint = ""
        if "unable to find a matching CMSIS-DAP device" in output or "no device found" in output.lower():
            hint = " The probe is not connected."
        elif "CMD_INFO failed" in output or "LIBUSB_ERROR_ACCESS" in output or "could not claim" in output.lower() or "busy" in output.lower():
            hint = " The probe is probably held by another debugger."
        raise RuntimeError("OpenOCD failed." + hint + "\n" + "\n".join(errors or output.splitlines()[-8:]))
    lines = [line[len(MARK) :].strip() for line in output.splitlines() if line.startswith(MARK)]
    return lines


def read_words(requests):
    """requests: list of (address, count). Returns a list of word lists, read without halting."""
    commands = [f'echo "{MARK} [read_memory {address:#x} 32 {count}]"' for address, count in requests]
    return [[int(word, 16) for word in line.split()] for line in run(commands)]


def partitions():
    """Firmware slot offsets from targets/partition_table.json: {'A': offset, 'B': offset}."""
    with open(os.path.join(REPO, "targets", "partition_table.json")) as file:
        table = json.load(file)
    offsets, offset = {}, FIRST_PARTITION_OFFSET
    for partition in table["partitions"]:
        name = partition["name"]
        if name.startswith("FW "):
            offsets[name[3:]] = offset
        size = partition["size"]
        offset += int(size[:-1]) * 1024 if size.endswith("K") else int(size, 0)
    return offsets


def parse_slot_header(words):
    """Describe the first sector of a slot: erased, or the picobin image definition found there."""
    data = struct.pack(f"<{len(words)}I", *words)
    if all(byte == 0xFF for byte in data):
        return {"state": "erased"}
    start = data.find(struct.pack("<I", PICOBIN_BLOCK_MARKER))
    if start < 0:
        return {"state": "no image header"}
    info = {"state": "valid"}
    pos = start + 4
    while pos + 4 <= len(data):
        item_type = data[pos] & 0x7F
        size = (data[pos + 1] | (data[pos + 2] << 8)) if data[pos] & 0x80 else data[pos + 1]
        if item_type == 0x42:
            flags = data[pos + 2] | (data[pos + 3] << 8)
            info["tbyb_pending"] = bool(flags & 0x8000)
        elif item_type == 0x48:
            minor, major = struct.unpack_from("<HH", data, pos + 4)
            info["version_major_word"], info["version_minor_word"] = major, minor
        elif item_type == 0x7F or size == 0:
            break
        pos += size * 4
    return info


def status():
    """Snapshot of the MCU, read without halting it."""
    slots = partitions()
    names = sorted(slots)
    requests = [(QMI_ATRANS0, 1), (TIMER0_TIMERAWL, 1), (DHCSR, 1)]
    requests += [(RAW_FLASH_BASE + slots[name], SECTOR // 4) for name in names]
    atrans, uptime, dhcsr, *headers = read_words(requests)
    active_sector = atrans[0] & 0xFFF
    active = next((name for name in names if slots[name] // SECTOR == active_sector), None)
    return {
        "active_slot": active,  # None: nothing booted, the core is in the bootrom
        "uptime_s": uptime[0] / 1e6,  # low 32 bits, wraps after ~71 minutes
        "halted": bool(dhcsr[0] & (1 << 17)),
        "sleeping": bool(dhcsr[0] & (1 << 18)),
        "slots": {name: dict(parse_slot_header(header), offset=slots[name]) for name, header in zip(names, headers)},
    }


def reset():
    run(["reset run"])


def resume():
    # OpenOCD resumes only targets it has seen halt, and resuming core0 resumes both
    halts = "; ".join(f"targets rp2350.dap.core{core}; halt; wait_halt 500" for core in (0, 1))
    run(["catch {" + halts + "}", "targets rp2350.dap.core0", "resume"])


def where():
    """Halt for a moment and return {core: (pc, lr)}; the cores run again afterwards.

    If the MCU was already halted (a crash, a breakpoint), it is left halted.
    """
    was_halted = status_halted()
    # Each core is halted through its own target: OpenOCD reads registers only
    # from a target it has seen halt. Resuming core0 resumes both.
    reads = []
    for core in (0, 1):
        reads += [f"targets rp2350.dap.core{core}", "halt", "wait_halt 500", f'echo "{MARK} {core} [reg pc] [reg lr]"']
    commands = ["catch {" + "; ".join(reads) + "} f1_error", "targets rp2350.dap.core0"]
    if not was_halted:
        commands += ["resume"]
    commands += ['if {$f1_error ne ""} { echo "Error: $f1_error" }']
    result = {}
    for line in run(commands):
        core, *rest = line.split()
        values = [int(word, 16) for word in rest if word.startswith("0x")]
        result[int(core)] = tuple(values[:2])
    return result


def status_halted():
    return bool(read_words([(DHCSR, 1)])[0][0] & (1 << 17))


def symbolize(addresses):
    """[(address, 'function at file:line')] from the ELF in build/, or '?' without toolchain or ELF."""
    names = dict(_symbolize([address for address in addresses if address >= XIP_BASE]))
    return [(address, names.get(address, "in the bootrom" if address < 0x8000 else "?")) for address in addresses]


def _symbolize(addresses):
    if not addresses:
        return []
    tool = sorted(glob.glob(os.path.expanduser("~/.pico-sdk/toolchain/*/bin/arm-none-eabi-addr2line")), reverse=True)
    elf = glob.glob(os.path.join(REPO, "build", "*.elf"))
    if not tool or not elf:
        return [(address, "?") for address in addresses]
    output = subprocess.run([tool[0], "-f", "-C", "-e", elf[0], *(f"{address:#x}" for address in addresses)], capture_output=True, text=True).stdout.splitlines()
    return [(address, f"{output[2 * i]} at {os.path.relpath(output[2 * i + 1], REPO) if output[2 * i + 1].startswith('/') else output[2 * i + 1]}") for i, address in enumerate(addresses)]


def flash_slot(slot, path):
    """Write a .bin to a firmware slot, verify it and reset. The MCU is halted meanwhile."""
    address = XIP_BASE + partitions()[slot.upper()]
    path = os.path.abspath(path)
    if not path.endswith(".bin"):
        raise ValueError("Give the .bin file: it is linked for 0x10000000 and runs from either slot")
    quoted = "{" + path + "}"
    run(["halt", f"flash write_image erase {quoted} {address:#x} bin", f"verify_image {quoted} {address:#x} bin", "reset run"], timeout=300)


def erase_slot_header(slot):
    """Erase the first sector of a slot, which makes its image unbootable."""
    address = XIP_BASE + partitions()[slot.upper()]
    run(["halt", f"flash erase_address {address:#x} {SECTOR:#x}", "reset run"])
