#!/usr/bin/env python3
"""
Updates memory_flash.incl from partition_table.json.
Run this after changing partition_table.json. scripts/flash.py and the agent tools
read the slot offsets from partition_table.json themselves.
"""

import argparse
import json
import re
import sys
from pathlib import Path

DEFAULT_JSON = "targets/partition_table.json"
DEFAULT_LD = "targets/memory_flash.incl"
FLASH_BASE = 0x10000000

def get_stripped(d, key, default=None):
    """
    Return value from dict ignoring extra/trailing spaces in keys.

    Example:
        "size ": "7680K "
    """
    if not isinstance(d, dict):
        return default

    key = key.strip().lower()

    for k, v in d.items():
        if isinstance(k, str) and k.strip().lower() == key:
            return v

    return default


def normalize(value):
    """
    Normalize text: strip, collapse spaces, uppercase.

    Example:
        " FW A " -> "FW A"
    """
    return " ".join(str(value).split()).upper()


def find_partition(partitions, wanted_name):
    """
    Find partition by normalized name.
    """
    wanted = normalize(wanted_name)

    for part in partitions:
        if not isinstance(part, dict):
            continue

        name = get_stripped(part, "name", "")
        if normalize(name) == wanted:
            return part

    raise ValueError(f"Partition '{wanted_name}' was not found in partition table")


def parse_size(value):
    """
    Parse sizes like:
        7680K
        7M
        1004KB
        7680KiB
        1024
        0x780000

    K/M/G are treated as 1024-based.
    """
    text = normalize(value).replace(" ", "")

    # Optional hex support, e.g. 0x780000
    if text.startswith("0X"):
        try:
            size = int(text, 16)
        except ValueError:
            raise ValueError(f"Cannot parse hex size: {value!r}")

        if size <= 0:
            raise ValueError(f"Invalid size: {value!r}")

        return size

    match = re.fullmatch(r"(\d+(?:\.\d+)?)([KMG]?)(?:I?B|I)?", text)
    if not match:
        raise ValueError(f"Cannot parse size: {value!r}")

    number = float(match.group(1))
    unit = match.group(2)

    multiplier = {
        "": 1,
        "K": 1024,
        "M": 1024 * 1024,
        "G": 1024 * 1024 * 1024,
    }[unit]

    size = int(round(number * multiplier))

    if size <= 0:
        raise ValueError(f"Invalid size: {value!r}")

    return size


def write_ld(path: Path, size: int):
    """
    Create memory_flash.incl with required FLASH region.
    """
    content = f"MEMORY {{FLASH(rx) : ORIGIN = 0x{FLASH_BASE:x}, LENGTH = 0x{size:x}}}\n"
    path.write_text(content, encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(
        description="Generate memory_flash.incl from partition_table.json"
    )

    parser.add_argument(
        "--json",
        dest="json_path",
        type=Path,
        default=DEFAULT_JSON,
        help=f"Path to partition table JSON, default: {DEFAULT_JSON}",
    )

    parser.add_argument(
        "--ld",
        dest="ld_path",
        type=Path,
        default=DEFAULT_LD,
        help=f"Output linker file, default: {DEFAULT_LD}",
    )

    args = parser.parse_args()

    try:
        json_text = args.json_path.read_text(encoding="utf-8-sig")
        data = json.loads(json_text)

        partitions = get_stripped(data, "partitions")
        if not isinstance(partitions, list):
            raise ValueError("JSON does not contain a valid 'partitions' list")

        partition = find_partition(partitions, "FW A")

        size_raw = get_stripped(partition, "size")
        if size_raw is None:
            raise ValueError(f"Partition does not contain 'size'")

        size_bytes = parse_size(size_raw)

        if (args.ld_path is not None):
            write_ld(args.ld_path, size_bytes)

        if size_bytes % 0x1000 != 0:
            print(
                f"Warning: partition size 0x{size_bytes:x} is not a multiple of 4K",
                file=sys.stderr,
            )

        print(
            f"FW partition size: "
            f"{size_bytes // 1024} K (0x{size_bytes:x})"
        )

    except Exception as exc:
        print(f"Error: {exc}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()