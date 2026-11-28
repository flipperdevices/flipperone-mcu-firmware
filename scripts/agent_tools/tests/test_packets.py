"""Packet codecs: UF2, picobin, console text, shell framing, U-Boot text, OpenOCD output."""

import struct

import pytest

from agent_tools.packets import host_shell, mcu_console, openocd_output, picobin, rpc, uboot, uf2


def uf2_block(number, total, address, payload):
    block = bytearray(512)
    struct.pack_into("<8I", block, 0, uf2.MAGIC_START0, uf2.MAGIC_START1, 0, address, len(payload), number, total, 0)
    block[32 : 32 + len(payload)] = payload
    struct.pack_into("<I", block, 508, uf2.MAGIC_END)
    return bytes(block)


def make_uf2(sections):
    """UF2 bytes for a list of sections, each a list of (address, payload)."""
    return b"".join(uf2_block(number, len(chunks), address, payload) for chunks in sections for number, (address, payload) in enumerate(chunks))


def test_uf2_parses_concatenated_sections():
    data = make_uf2([[(0x10FFFF00, b"boot")], [(0x10000000, b"a" * 256), (0x10000100, b"b" * 256)]])
    chunks = uf2.parse_blocks(data)
    assert [address for address, _ in chunks] == [0x10000000, 0x10000100, 0x10FFFF00]


def test_uf2_rejects_corrupt_magic():
    data = bytearray(make_uf2([[(0x10000000, b"x")]]))
    data[0] = 0
    with pytest.raises(ValueError, match="corrupt"):
        uf2.parse_blocks(bytes(data))


def test_uf2_rejects_gap_and_duplicate_blocks():
    with pytest.raises(ValueError, match="gap or overlap"):
        uf2.parse_blocks(make_uf2([[(0x10000000, b"a" * 256), (0x10000200, b"b" * 256)]]))
    blocks = bytearray(make_uf2([[(0x10000000, b"a" * 256), (0x10000100, b"b" * 256)]]))
    blocks += blocks[512:]  # a third block that repeats the section's block number 1
    with pytest.raises(ValueError, match="appears twice"):
        uf2.parse_blocks(bytes(blocks))


def test_uf2_contiguous():
    chunks = uf2.parse_blocks(make_uf2([[(0x10002000, b"a" * 256), (0x10002100, b"b" * 256)]]))
    start, payload = uf2.contiguous(chunks)
    assert start == 0x10002000 and payload == b"a" * 256 + b"b" * 256
    with pytest.raises(ValueError, match="not contiguous"):
        uf2.contiguous([(0, b"a"), (4, b"b")])


def image_def_bytes(tbyb=True, major=3):
    flags = 0x8000 if tbyb else 0
    data = bytearray(struct.pack("<I", picobin.PICOBIN_BLOCK_MARKER))
    data += bytes([0x42, 4, flags & 0xFF, flags >> 8]) + b"\x00" * 12
    data += bytes([0x48, 4, 0, 0]) + struct.pack("<HH", 0, major) + b"\x00" * 8
    data += bytes([0x7F, 0, 0, 0])
    return bytes(data)


def test_picobin_parse_image_def():
    info = picobin.parse_image_def(image_def_bytes(tbyb=True, major=3))
    assert info == {"state": "valid", "tbyb_pending": True, "version_major_word": 3, "version_minor_word": 0}
    assert picobin.parse_image_def(image_def_bytes(tbyb=False))["tbyb_pending"] is False
    assert picobin.parse_image_def(b"\x00" * 64)["state"] == "no image header"
    erased = picobin.parse_slot_header([0xFFFFFFFF] * 8)
    assert erased["state"] == "erased"


def test_mcu_console_text():
    assert mcu_console.at_prompt(b"boot\r\ncontrol>: ")
    assert not mcu_console.at_prompt(b"boot\r\nrunning")
    assert mcu_console.clean(b"\x1b[1mbold\x1b[0m\r\n") == "bold\n"


def test_rpc_varint_roundtrip():
    message = rpc.field_varint(1, 300) + rpc.field_bytes(2, b"abc")
    fields = rpc.parse_message(message)
    assert fields[1] == 300 and fields[2] == b"abc"
    assert rpc.varint(0) == b"\x00" and rpc.varint(300) == bytes([0xAC, 0x02])


def test_rpc_rle_decode():
    # a run of 5 x 0xFF, then 3 bytes as they are, then a run of 4 x 0x00
    assert rpc.rle_decode(bytes([0x05, 0xFF, 0x83, 1, 2, 3, 0x04, 0x00])) == b"\xff" * 5 + b"\x01\x02\x03" + b"\x00" * 4
    assert rpc.rle_decode(b"") == b""
    with pytest.raises(ValueError, match="verbatim"):
        rpc.rle_decode(bytes([0x83, 1]))
    with pytest.raises(ValueError, match="repeated"):
        rpc.rle_decode(bytes([0x05]))


def test_host_shell_framing():
    assert host_shell.strip_probe_banner(b"fixed baud rate 1500000\nuser@f:~$ ") == b"user@f:~$"
    assert host_shell.SHELL_PROMPT.search(b"user@flipper:~$ ")
    body, code = host_shell.split_done(b"\r\nhello\r\n__F1_DONE__3\r\n")
    assert body == b"\r\nhello\r\n" and code == 3
    assert host_shell.parse_uptime("noise\n42.5\n") == 42.5
    with pytest.raises(RuntimeError, match="Could not read the host uptime"):
        host_shell.parse_uptime("noise")


def test_uboot_entries():
    scan = "scan: /boot/loader/entries/1-flipperos-Desktop-7.4.conf /boot/loader/entries/2-router-6.conf"
    entries = uboot.parse_boot_entries(scan)
    assert entries == ["Desktop-7.4", "router-6"]
    assert uboot.entry_position("1", entries) == 1
    assert uboot.entry_position("ROUTER", entries) == 2
    with pytest.raises(RuntimeError, match="matches 0"):
        uboot.entry_position("nope", entries)
    with pytest.raises(RuntimeError, match="no boot entry 9"):
        uboot.entry_position("9", entries)


def test_openocd_output():
    output = 'Info : x\nF1> 0x000002fe 0x00000001\nError: CMD_INFO failed\nError: CMD_INFO failed\n'
    assert openocd_output.extract_marks(output) == ["0x000002fe 0x00000001"]
    assert openocd_output.error_lines(output) == ["Error: CMD_INFO failed"]
    assert "held by another debugger" in openocd_output.failure_hint(output)
    assert openocd_output.failure_hint("no device found") == " The probe is not connected."
    assert openocd_output.failure_hint("") == ""
