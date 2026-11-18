"""UF2 container: block parsing and Intel HEX output for OpenOCD."""

import struct

MAGIC_START0 = 0x0A324655
MAGIC_START1 = 0x9E5D5157
MAGIC_END = 0x0AB16F30
BLOCK_SIZE = 512
MAX_PAYLOAD = 476


def parse_sections(data):
    """[{total, blocks: {number: (address, payload)}}] in file order.

    picotool concatenates independent UF2 sections (a boot-info part, then the
    image), each numbered from zero; every section is validated on its own, and
    one that ships fewer blocks than it declares is returned as it is so that
    the caller can tell truncated files from block sets it does not care about.
    """
    if not data or len(data) % BLOCK_SIZE:
        raise ValueError(f"UF2 size {len(data)} is not a positive multiple of {BLOCK_SIZE}")
    sections, current, total = [], None, None
    for offset in range(0, len(data), BLOCK_SIZE):
        start0, start1, _flags, address, payload_size, block_no, block_total, _family = struct.unpack_from("<8I", data, offset)
        end = struct.unpack_from("<I", data, offset + BLOCK_SIZE - 4)[0]
        if start0 != MAGIC_START0 or start1 != MAGIC_START1 or end != MAGIC_END:
            raise ValueError(f"UF2 block at offset {offset} is corrupt")
        if payload_size > MAX_PAYLOAD:
            raise ValueError(f"UF2 block {block_no} has {payload_size} payload bytes, more than {MAX_PAYLOAD}")
        if block_no == 0:
            if current:
                sections.append({"total": total, "blocks": current})
            current, total = {}, block_total
        if total is None:
            raise ValueError("UF2 file does not start with block 0")
        if block_total != total:
            raise ValueError(f"UF2 block {block_no} says {block_total} blocks, the section has {total}")
        if block_no >= total:
            raise ValueError(f"UF2 block number {block_no} is outside 0..{total - 1}")
        if block_no in current:
            raise ValueError(f"UF2 block {block_no} appears twice")
        current[block_no] = (address, data[offset + 32 : offset + 32 + payload_size])
    if current:
        sections.append({"total": total, "blocks": current})
    for section in sections:
        ordered = [section["blocks"][number] for number in sorted(section["blocks"])]
        for (address, payload), (next_address, _) in zip(ordered, ordered[1:]):
            if address + len(payload) != next_address:
                raise ValueError(f"UF2 section has a gap or overlap before 0x{next_address:08x}")
    return sections


def parse_blocks(data):
    """[(address, payload)] sorted by address, over every section."""
    chunks = [chunk for section in parse_sections(data) for chunk in section["blocks"].values()]
    chunks.sort()
    for (address, payload), (next_address, _) in zip(chunks, chunks[1:]):
        if address + len(payload) > next_address:
            raise ValueError(f"UF2 chunks overlap at 0x{address:08x}")
    return chunks


def contiguous(chunks):
    """One payload for a single contiguous run of chunks; raises when there are gaps."""
    if not chunks:
        return None, b""
    start = chunks[0][0]
    payload = b"".join(part for _, part in chunks)
    expected = start
    for address, part in chunks:
        if address != expected:
            raise ValueError(f"UF2 data is not contiguous: 0x{address:08x} follows 0x{expected:08x}; write the .bin instead")
        expected += len(part)
    return start, payload
