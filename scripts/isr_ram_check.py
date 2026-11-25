#!/usr/bin/env python3
"""
isr_ram_check.py: check that the I2C slave ISR path touches no flash.

The I2C slave interrupt runs above configMAX_SYSCALL_INTERRUPT_PRIORITY, so
FURI_CRITICAL does not mask it and it keeps running while flash is erased or
programmed (this is what keeps the update over I2C working, #263). Everything
it reaches must then be in RAM: code, and the data it reads. Functions on that
path are defined with I2C_ISR_FUNC (lib/drivers/i2c_slave/i2c_slave.h), which
places them in RAM and leaves a marker symbol __i2c_isr_func_<name>.

This tool checks the linked ELF:

  1. Every function in REQUIRED exists and is marked. These are reached
     through function pointers, which the disassembly cannot follow: if one
     lost its I2C_ISR_FUNC in a rewrite, nothing would start the check from it.
  2. Starting from every marked function, it follows direct calls (`bl`,
     tail calls `b`, also conditional and `.n`/`.w`) and RAM veneers (`ldr.w pc, [pc]` + `.word TARGET`).
     Every function reached must be marked with I2C_ISR_FUNC and be in RAM.
  3. Literal pools of those functions: a `.word` that points into flash data is
     an address the ISR may load from (a const table, a GpioPin, a string).
     Function addresses are skipped: the ISR may pass them on (to the
     deferred-call ring) without calling them.
  4. Indirect calls (`blx reg`, tail call `bx reg`) are allowed only in DISPATCHERS. A new one
     means a new function pointer on the path: its targets must be marked and
     added to REQUIRED, and the caller to DISPATCHERS.

Functions reachable only on a crash path (furi_check failing) are allowed: the
system halts there anyway.

When it fails:
  - "not marked": define the function with I2C_ISR_FUNC, or keep it off the ISR
    path: writes are applied later, by the register software interrupt.
  - "in FLASH": library code (memcpy, FreeRTOS) cannot run on the path at all.
  - "flash data": copy the value to RAM at init (see the interrupt line pins in
    i2c_registers.c) or place the object in RAM with __not_in_flash("name").
  - "REQUIRED ... missing": a callback was renamed, removed or lost its
    I2C_ISR_FUNC. Restore the macro, or update REQUIRED to the new callback.
  If the path does reach flash anyway, the MCU locks up on the first I2C
  interrupt during a flash erase: the log stops at the flash operation with no
  crash report, CFSR = 0x100 (IBUSERR), HFSR = 0x40000000, DHCSR has S_LOCKUP
  set, and flash is unreadable over SWD until a reset.

Runs as a POST_BUILD step of the firmware (CMakeLists.txt), which passes the
toolchain's nm and objdump. By hand:
  python3 scripts/isr_ram_check.py [--nm PATH] [--objdump PATH] [path/to/firmware.elf]
Without --nm/--objdump the tools are taken from ARM_TOOLCHAIN_BIN, PATH (where
fbtenv puts the toolchain) or the Pico SDK toolchain in ~/.pico-sdk.

Exit code 0 when the path is clean, 1 otherwise.
"""

import argparse
import glob
import os
import re
import shutil
import subprocess
import sys
from collections import defaultdict, deque

DEFAULT_ELF = "build/f2-release/flipperone-mcu-firmware.elf"
MARKER_PREFIX = "__i2c_isr_func_"

# Reached through function pointers, so the call graph cannot find them: each
# must exist and carry I2C_ISR_FUNC.
REQUIRED = {
    "i2c_slave_irq_callback": "I2C1 vector, set with irq_set_exclusive_handler (i2c_slave.c)",
    "furi_hal_i2c_bus_cpu_slave_callback": "I2cSlave.callback (furi_hal_i2c_config.c)",
    "i2c_intercom_isr": "FuriHalI2cBus.api.slave.callback (i2c_intercom.c)",
    "furi_hal_i2c_bus_slave_read": "FuriHalI2cBus.api.slave.read_blocking (furi_hal_i2c_config.c)",
    "furi_hal_i2c_bus_slave_write": "FuriHalI2cBus.api.slave.write_blocking (furi_hal_i2c_config.c)",
}

# Functions allowed to call through a pointer on the path. Their targets are
# the REQUIRED callbacks above.
DISPATCHERS = {
    "i2c_slave_irq_callback",  # I2cSlave.callback
    "i2c_slave_handle_stop",  # I2cSlave.callback
    "furi_hal_i2c_bus_cpu_slave_callback",  # api.slave.callback
    "furi_hal_i2c_slave_read_blocking",  # api.slave.read_blocking
    "furi_hal_i2c_slave_write_blocking",  # api.slave.write_blocking
}

# Reached only when a check fails; the system halts there.
CRASH_ONLY = re.compile(r"^(__furi_crash|__furi_halt|furi_crash|furi_halt)")

RAM_RANGES = [
    (0x20000000, 0x20082000),  # SRAM + SCRATCH_X + SCRATCH_Y
    (0x13FFC000, 0x14000000),  # XIP cache used as RAM
]
FLASH_RANGES = [
    (0x10000000, 0x18000000),  # XIP windows: cached, uncached, translated
    (0x1C000000, 0x1E000000),
]


def classify(addr):
    if any(lo <= addr < hi for lo, hi in RAM_RANGES):
        return "RAM"
    if any(lo <= addr < hi for lo, hi in FLASH_RANGES):
        return "FLASH"
    return "OTHER"


def base_name(name):
    """Strip GCC clone suffixes: foo.constprop.0, foo.isra.0, foo.part.0."""
    return name.split(".", 1)[0]


def find_tool(name):
    env = os.environ.get("ARM_TOOLCHAIN_BIN")
    if env:
        return os.path.join(env, name)
    found = shutil.which(name)
    if found:
        return found
    candidates = sorted(glob.glob(os.path.expanduser(f"~/.pico-sdk/toolchain/*/bin/{name}")))
    if candidates:
        return candidates[-1]
    sys.exit(f"error: {name} not found: put it on PATH or set ARM_TOOLCHAIN_BIN")


def main():
    parser = argparse.ArgumentParser(description="Check that the I2C slave ISR path touches no flash")
    parser.add_argument("elf", nargs="?", default=DEFAULT_ELF, help=f"firmware ELF (default: {DEFAULT_ELF})")
    parser.add_argument("--nm", help="arm-none-eabi-nm to use")
    parser.add_argument("--objdump", help="arm-none-eabi-objdump to use")
    parser.add_argument("--quiet", action="store_true", help="print only problems and the verdict")
    args = parser.parse_args()
    nm_tool = args.nm or find_tool("arm-none-eabi-nm")
    objdump_tool = args.objdump or find_tool("arm-none-eabi-objdump")

    # --- symbols ---
    funcs = {}  # name -> address (thumb bit cleared)
    marked = {}  # function name -> marker address
    data_syms = []
    for line in subprocess.check_output([nm_tool, "--defined-only", args.elf], text=True).splitlines():
        m = re.match(r"^([0-9a-f]+) (\w) (\S+)$", line)
        if not m:
            continue
        addr, kind, name = int(m.group(1), 16) & ~1, m.group(2), m.group(3)
        if name.startswith(MARKER_PREFIX):
            marked[name[len(MARKER_PREFIX):]] = addr
        elif kind in "Tt":
            funcs[name] = addr
        elif kind in "RrDdBb":
            data_syms.append((addr, name))
    data_syms.sort()
    addr_to_name = {}
    for name, addr in sorted(funcs.items()):
        addr_to_name.setdefault(addr, name)

    def data_name(addr):
        best = None
        for a, n in data_syms:
            if a > addr:
                break
            best = (a, n)
        if best is None:
            return "?"
        return best[1] if best[0] == addr else f"{best[1]}+0x{addr - best[0]:x}"

    def label(name):
        # objdump may label a marked function with its marker symbol
        return name[len(MARKER_PREFIX):] if name.startswith(MARKER_PREFIX) else name

    # --- disassembly: calls, indirect calls, veneers, literals ---
    callgraph = defaultdict(set)
    indirect = defaultdict(list)
    veneers = {}
    literals = defaultdict(list)
    current = None
    pending_veneer = None
    objdump = subprocess.check_output([objdump_tool, "-d", args.elf], text=True)
    for line in objdump.splitlines():
        m = re.match(r"^[0-9a-f]+ <(.+)>:$", line)
        if m:
            current = label(m.group(1))
            pending_veneer = None
            continue
        if current is None:
            continue
        m = re.search(r"\t(?:bl|b(?:eq|ne|cs|hs|cc|lo|mi|pl|vs|vc|hi|ls|ge|lt|gt|le)?(?:\.[nw])?)\s+[0-9a-f]+ <([^>+]+)>", line)
        if m:
            callee = label(m.group(1))
            if callee != current:
                callgraph[current].add(callee)
            continue
        m = re.search(r"\t(?:blx\s+(r\d+|ip|lr)|bx\s+(r\d+|ip))\b", line)
        if m:
            a = re.match(r"^\s*([0-9a-f]+):", line)
            indirect[current].append((int(a.group(1), 16) if a else 0, m.group(1) or m.group(2)))
            continue
        if re.search(r"ldr\.w\s+pc,\s*\[pc\]", line):
            pending_veneer = current
            continue
        m = re.match(r"^\s*([0-9a-f]+):\s+([0-9a-f]{8})\s+\.word\s+0x([0-9a-f]+)", line)
        if m:
            value = int(m.group(3), 16)
            if pending_veneer is not None:
                veneers[pending_veneer] = value & ~1
                pending_veneer = None
            else:
                literals[current].append((int(m.group(1), 16), value))

    problems = []

    # --- 1. required pointer-reached callbacks ---
    for name, why in REQUIRED.items():
        if name not in funcs:
            problems.append(f"REQUIRED {name} missing: {why}. Renamed or removed? Update REQUIRED")
        elif name not in marked:
            problems.append(f"REQUIRED {name} is not marked with I2C_ISR_FUNC: {why}")

    # --- 2. closure from every marked function ---
    reached = {}  # name -> how it was reached
    queue = deque((name, "I2C_ISR_FUNC") for name in sorted(marked))
    while queue:
        f, via = queue.popleft()
        if f in reached:
            continue
        reached[f] = via
        if CRASH_ONLY.match(f):
            continue
        addr = funcs.get(f)
        if addr is None:
            problems.append(f"{f} (called from {via}): no symbol, review by hand")
            continue
        loc = classify(addr)
        if f in veneers:
            # Linker veneer: transparent, check what it jumps to
            target = addr_to_name.get(veneers[f], f"0x{veneers[f]:x}")
            queue.append((target, via))
            continue
        if loc != "RAM":
            problems.append(f"{f} (called from {via}) is in {loc} at 0x{addr:08x}")
            continue
        if base_name(f) not in marked:
            problems.append(f"{f} (called from {via}) is not marked with I2C_ISR_FUNC")
        for callee in sorted(callgraph.get(f, ())):
            queue.append((callee, f))

    path = sorted(f for f in reached if f in funcs and f not in veneers and not CRASH_ONLY.match(f))

    # --- 3. flash-resident data loaded on the path ---
    for f in path:
        for site, value in literals.get(f, []):
            if classify(value) == "FLASH" and (value & ~1) not in addr_to_name:
                problems.append(f"{f} @ 0x{site:08x} loads flash data 0x{value:08x} {data_name(value)}")

    # --- 4. indirect calls only at known dispatch points ---
    for f in path:
        if base_name(f) in DISPATCHERS:
            continue
        for site, reg in indirect.get(f, []):
            problems.append(
                f"{f} @ 0x{site:08x}: indirect call through {reg} outside DISPATCHERS: "
                "mark its targets with I2C_ISR_FUNC and add them to REQUIRED, the caller to DISPATCHERS"
            )

    if not args.quiet:
        print(f"I2C ISR path: {len(marked)} marked, {len(path)} reached\n")
        for f in path:
            print(f"  ok   {f:48s} 0x{funcs[f]:08x}")
        print()
    for p in problems:
        print(f"  !!   {p}")
    if problems:
        print(f"FAIL: {len(problems)} problem(s) on the I2C ISR path, see the header of scripts/isr_ram_check.py")
        sys.exit(1)
    print(f"PASS: the I2C ISR path is RAM-only ({len(path)} functions, {len(marked)} marked)")


if __name__ == "__main__":
    main()
