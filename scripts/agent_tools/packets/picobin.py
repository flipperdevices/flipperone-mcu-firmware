"""Picobin image definition, as found in the first sector of a firmware slot."""

import struct

PICOBIN_BLOCK_MARKER = 0xFFFFDED3


def parse_image_def(data):
    """Describe the picobin image definition in a blob: state, version words, TBYB flag."""
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


def parse_slot_header(words):
    """Describe the first sector of a slot: erased, or the picobin image definition found there."""
    data = struct.pack(f"<{len(words)}I", *words)
    if all(byte == 0xFF for byte in data):
        return {"state": "erased"}
    return parse_image_def(data)
