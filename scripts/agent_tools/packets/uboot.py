"""U-Boot boot menu text."""

import re

ENTRY_PATTERN = re.compile(r"/loader/entries/(?:\d+-)?(?:flipperos-)?(\S+)\.conf")


def parse_boot_entries(scan):
    """Entry names from the U-Boot scan output, in menu order."""
    return ENTRY_PATTERN.findall(scan)


def entry_position(wanted, entries):
    """Menu position (from 1) for --entry: a number, or text found in exactly one entry name."""
    if wanted.isdigit():
        position = int(wanted)
        if position < 1 or (entries and position > len(entries)):
            raise RuntimeError(f"There is no boot entry {position}; the host has {len(entries)}")
        return position
    matching = [number for number, name in enumerate(entries, 1) if wanted.lower() in name.lower()]
    if len(matching) != 1:
        raise RuntimeError(f"--entry '{wanted}' matches {len(matching)} boot entries, it has to match one; the host is powered off again")
    return matching[0]
