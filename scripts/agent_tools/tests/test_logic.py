"""Logic layer against fake ports: console, RPC, Linux host, device session, flashing."""

import struct
import time
import types
from pathlib import Path

import pytest

from agent_tools.packets import rpc
from agent_tools.logic.console import McuConsole
from agent_tools.logic.device import DeviceSession
from agent_tools.logic.linux import LinuxHost
from agent_tools.logic.rpc import RpcSession

from _fake import FakePort, console_responder, linux_responder


def make_session(responder=console_responder):
    opened = []

    def open_port(role):
        port = FakePort(responder)
        opened.append((role, port))
        return port

    return DeviceSession("mcu", open_port=open_port), opened


def test_console_sync_and_command():
    port = FakePort(console_responder)
    console = McuConsole(port)
    console.sync()
    assert console.command("uptime") == "reply"


def test_console_command_timeout():
    port = FakePort()
    console = McuConsole(port)
    with pytest.raises(TimeoutError, match="did not finish"):
        console.command("uptime", timeout=0.05)


def question_responder(port, data):
    if data == b"otp program 1 aa\r":
        port.feed(b"otp program 1 aa\r\nWarning! This operation is irreversible! Are you sure? y/n\r\n")
    elif data == b"y":
        port.feed(b"Block 1: written\r\ncontrol>: ")


def test_console_command_answers_a_question():
    port = FakePort(question_responder)
    reply = McuConsole(port).command("otp program 1 aa", timeout=1, answer="y")
    assert port.written[-1] == b"y"
    assert reply.endswith("Block 1: written")


def test_console_command_question_without_answer_times_out():
    port = FakePort(question_responder)
    with pytest.raises(TimeoutError, match="did not finish"):
        McuConsole(port).command("otp program 1 aa", timeout=0.05)
    assert b"y" not in port.written


def test_console_sync_failure():
    with pytest.raises(RuntimeError, match="does not show a prompt"):
        McuConsole(FakePort()).sync()


def test_device_session_opens_once_and_reuses_the_console():
    session, opened = make_session()
    try:
        assert session.command("uptime") == "reply"
        assert session.app() is None
        session.command("desktop get_run_app")
    finally:
        session.close()
    assert len(opened) == 1
    assert opened[0][1].closed


def test_device_session_app_running():
    def responder(port, data):
        if data.startswith(b"desktop get_run_app"):
            port.feed(data + b"\r\nrunning app: self_check\r\ncontrol>: ")
        else:
            console_responder(port, data)

    session, _ = make_session(responder)
    assert session.app() == "self_check"
    session.close()


def test_linux_host_ready_and_exec():
    host = LinuxHost(open_port=lambda: FakePort(linux_responder))
    assert host.ready() == (True, True)
    assert host.exec("echo hello") == ("hello", 0)
    host.close()


def test_rpc_session_screenshot_and_close():
    width, height, pixels = 2, 2, b"\x01\x02\x03\x04"
    frame = (
        rpc.field_varint(1, width)
        + rpc.field_varint(2, height)
        + rpc.field_varint(3, 0)
        + rpc.field_bytes(5, pixels)
    )
    message = rpc.field_bytes(rpc.FIELD_FRAME, frame)
    reply = rpc.varint(len(message)) + message
    start = rpc.varint(2) + rpc.field_bytes(rpc.FIELD_START_VIRTUAL_DISPLAY, b"")
    assert start == rpc.varint(2) + rpc.field_bytes(rpc.FIELD_START_VIRTUAL_DISPLAY, b"")

    def responder(port, data):
        if data == start:
            port.feed(reply)
        else:
            console_responder(port, data)

    port = FakePort(responder)
    with RpcSession(port) as session:
        assert session.screenshot() == (width, height, pixels)
        session.button("ok", "press")
        session.touch("start", 10, 20)
    assert not session.port_lost


def test_flash_preflight_bin(tmp_path):
    from agent_tools import constants
    from agent_tools.logic import flash, swd
    from test_packets import image_def_bytes

    offset, size = swd.partition_bounds("A")
    path = tmp_path / "fw.bin"
    path.write_bytes(image_def_bytes(tbyb=True) + b"\x00" * 512)
    plan = flash.preflight("a", str(path))
    assert plan["slot"] == "A" and plan["kind"] == "bin"
    assert plan["address"] == constants.XIP_BASE + offset
    assert plan["payload_address"] == plan["address"]
    assert plan["size"] == size
    assert plan["image"]["tbyb_pending"] is True

    sparse = tmp_path / "big.bin"
    with open(sparse, "wb") as file:
        file.truncate(size + 1)
    with pytest.raises(ValueError, match="larger than slot"):
        flash.preflight("A", str(sparse))


def test_flash_preflight_uf2(tmp_path):
    from agent_tools import constants
    from agent_tools.logic import flash, swd
    from test_packets import image_def_bytes, make_uf2

    slot = constants.XIP_BASE + swd.partition_bounds("A")[0]
    outside = tmp_path / "outside.uf2"
    outside.write_bytes(make_uf2([[(constants.XIP_BASE, image_def_bytes())]]))
    with pytest.raises(ValueError, match="outside slot"):
        flash.preflight("a", str(outside))

    inside = tmp_path / "inside.uf2"
    inside.write_bytes(make_uf2([[(slot, image_def_bytes(tbyb=True) + b"\x00" * 128)]]))
    plan = flash.preflight("A", str(inside))
    assert plan["kind"] == "uf2" and plan["payload_address"] == slot
    assert plan["image"]["tbyb_pending"] is True


def test_board_config_owns_the_core_and_reading_helpers():
    from agent_tools.logic import swd

    text = open(swd.CONFIG).read()
    assert "source [find target/rp2350.cfg]" in text
    # Core names come from the sourced config: OpenOCD builds name them differently
    assert "_TARGETNAME_" in text
    assert "set ::F1_CORE0 rp2350" not in text
    for proc in ("f1_core", "f1_core_name", "f1_halted", "f1_halt_cores", "f1_resume", "f1_where", "f1_read", "f1_stage_tbyb"):
        assert f"proc {proc}" in text


def test_swd_where_and_resume_go_through_the_board_config(monkeypatch):
    from agent_tools.logic import swd
    from agent_tools.packets import openocd_output

    calls = []

    def fake_run(lines, timeout=None):
        calls.append(lines)
        return ["0 0x10001000 0x10001fff", "1 0x0000019e 0x00000203"]

    monkeypatch.setattr(swd, "run", fake_run)
    assert swd.where() == {0: (0x10001000, 0x10001FFF), 1: (0x19E, 0x203)}
    assert calls == [[f"f1_where {openocd_output.MARK}"]]
    calls.clear()
    swd.resume()
    assert calls == [["f1_resume"]]


def test_swd_read_words_goes_through_the_board_config(monkeypatch):
    from agent_tools.logic import swd
    from agent_tools.packets import openocd_output

    calls = []

    def fake_run(lines, timeout=None):
        calls.append(lines)
        return ["04000002", "deadbeef 0"]

    monkeypatch.setattr(swd, "run", fake_run)
    assert swd.read_words([(0x400D0034, 1), (0x10000000, 2)]) == [[0x04000002], [0xDEADBEEF, 0]]
    assert calls == [[f"f1_read 0x400d0034 1 {openocd_output.MARK}", f"f1_read 0x10000000 2 {openocd_output.MARK}"]]


def test_flash_stages_a_tbyb_image_through_the_board_config(tmp_path, monkeypatch):
    from agent_tools.logic import flash, swd
    from test_packets import image_def_bytes

    commands = []
    monkeypatch.setattr(swd, "run", lambda lines, timeout=None: commands.append(lines))
    monkeypatch.setattr(flash, "wait_for_buy", lambda plan: {"state": "valid"})
    tbyb = tmp_path / "tbyb.bin"
    tbyb.write_bytes(image_def_bytes(tbyb=True) + b"\x00" * 64)
    plan = flash.flash("a", str(tbyb), activate=True)
    assert plan["staged"] is True and plan["tbyb"] is True
    # The vector goes into the scratch, then the reset enters the flash-update boot
    assert commands[0][-2:] == [f"f1_stage_tbyb {plan['address']:#x}", "reset run"]
    assert not any(line.startswith(("mww", "targets")) for line in commands[0])

    plain = tmp_path / "plain.bin"
    plain.write_bytes(image_def_bytes(tbyb=False) + b"\x00" * 64)
    commands.clear()
    assert flash.flash("a", str(plain), activate=True)["staged"] is False
    assert "reset run" in commands[0]
    assert not any(line.startswith("f1_stage_tbyb") for line in commands[0])


def test_wait_for_buy_polls_until_the_bootrom_buys(monkeypatch):
    from agent_tools import constants
    from agent_tools.logic import flash

    # The image definition is at byte 312 of current builds, not at the start
    assert flash.HEADER_WORDS * 4 == constants.SECTOR
    plan = {"slot": "A", "offset": 0x2000}
    images = [{"state": "erased"}, {"state": "valid", "tbyb_pending": True}, {"state": "valid", "tbyb_pending": False}]
    monkeypatch.setattr(flash, "slot_image", lambda plan: images.pop(0))
    monkeypatch.setattr(flash, "time", types.SimpleNamespace(monotonic=time.monotonic, sleep=lambda seconds: None))
    assert flash.wait_for_buy(plan)["tbyb_pending"] is False

    monkeypatch.setattr(flash, "slot_image", lambda plan: {"state": "valid", "tbyb_pending": True})
    with pytest.raises(RuntimeError, match="unbought"):
        flash.wait_for_buy(plan, timeout=0)


def test_uf2_payload_sections_must_be_complete(tmp_path):
    from agent_tools import constants
    from agent_tools.logic import flash, swd
    from test_packets import image_def_bytes, uf2_block

    slot = constants.XIP_BASE + swd.partition_bounds("A")[0]
    truncated = tmp_path / "truncated.uf2"
    truncated.write_bytes(uf2_block(0, 2, slot, image_def_bytes(tbyb=True) + b"\x00" * 128))
    with pytest.raises(ValueError, match="truncated"):
        flash.preflight("a", str(truncated))

    # An incomplete section outside the slot is the boot-info block picotool writes: not payload
    bootinfo = tmp_path / "bootinfo.uf2"
    bootinfo.write_bytes(uf2_block(0, 2, 0x10FFFF00, b"\x00" * 16) + uf2_block(0, 1, slot, image_def_bytes(tbyb=True) + b"\x00" * 128))
    plan = flash.preflight("a", str(bootinfo))
    assert plan["kind"] == "uf2" and plan["payload_address"] == slot


def test_uf2_sections_outside_the_slot_are_refused(tmp_path):
    from agent_tools import constants
    from agent_tools.logic import flash, swd
    from test_packets import image_def_bytes, uf2_block

    slot_a = constants.XIP_BASE + swd.partition_bounds("A")[0]
    slot_b = constants.XIP_BASE + swd.partition_bounds("B")[0]
    payload = image_def_bytes(tbyb=True) + b"\x00" * 128
    split = tmp_path / "split.uf2"
    split.write_bytes(uf2_block(0, 1, slot_a, payload) + uf2_block(0, 1, slot_b, payload))
    with pytest.raises(ValueError, match="outside slot"):
        flash.preflight("a", str(split))


def test_flash_writes_the_bytes_preflight_checked(tmp_path, monkeypatch):
    from agent_tools.logic import flash, swd
    from test_packets import image_def_bytes

    source = tmp_path / "fw.bin"
    image = image_def_bytes(tbyb=False) + b"\x00" * 64
    source.write_bytes(image)
    written = {}

    def fake_run(lines, timeout=None):
        quoted = next(line for line in lines if line.startswith("flash write_image"))
        path = quoted.split(" erase ", 1)[1].split(" ", 1)[0].strip("{}")
        written["payload"] = open(path, "rb").read()
        written["is_source"] = path == str(source)

    monkeypatch.setattr(swd, "run", fake_run)
    plan = flash.preflight("a", str(source))
    source.write_bytes(b"another build landed here")  # a rebuild during the flash
    flash.write(plan)
    assert written["payload"] == image
    assert written["is_source"] is False


def test_freeze_host_releases_when_arming_fails(monkeypatch):
    from agent_tools.logic import flash

    calls = []

    class Probe:
        def __enter__(self):
            return self

        def __exit__(self, *exception):
            return False

        def command(self, line, timeout=None):
            calls.append(line)
            if line == "gpio cpu_reset 0":
                raise TimeoutError(f"no reply to {line}")

    monkeypatch.setattr(flash, "DeviceSession", lambda role: Probe())
    with pytest.raises(TimeoutError):
        with flash.freeze_host():
            pytest.fail("the body must not run when the host was not held")
    assert calls == ["gpio cpu_reset 0", "gpio cpu_reset 1"]


def test_freeze_host_reports_a_failed_release_and_keeps_the_write_failure(monkeypatch):
    from agent_tools.logic import flash

    class Probe:
        def __enter__(self):
            return self

        def __exit__(self, *exception):
            return False

        def command(self, line, timeout=None):
            if line == "gpio cpu_reset 1":
                raise TimeoutError("no reply to the release")

    monkeypatch.setattr(flash, "DeviceSession", lambda role: Probe())
    with pytest.raises(RuntimeError, match="the write failed") as caught:
        with flash.freeze_host():
            raise ValueError("the write failed")
    assert "releasing cpu_reset also failed" in str(caught.value)
    assert isinstance(caught.value.__cause__, ValueError)


def test_console_send_raw_writes_bytes_as_they_are():
    port = FakePort(lambda p, data: p.feed(b"echo\r\n") if data == b"log\r" else None)
    console = McuConsole(port)
    assert console.send_raw(b"log\r") == "echo"
    assert console.send_raw(b"x" * 3) == ""
    assert port.written == [b"log\r", b"xxx"]


def test_openocd_of_the_toolchain_bundle_comes_before_the_pico_sdk_one(tmp_path, monkeypatch):
    from agent_tools.logic import swd

    def fake_openocd(base):
        (base / "scripts" / "target").mkdir(parents=True)
        (base / "scripts" / "target" / "rp2350.cfg").write_text("")
        (base / "openocd").write_text("")
        return str(base / "openocd")

    bundle = fake_openocd(tmp_path / "bundle" / "tools" / "openocd")
    pico = fake_openocd(tmp_path / "home" / ".pico-sdk" / "openocd" / "0.12.0+dev")
    monkeypatch.setenv("HOME", str(tmp_path / "home"))
    monkeypatch.setattr(swd, "REPO", str(tmp_path))
    monkeypatch.delenv("F1_OPENOCD", raising=False)
    monkeypatch.delenv("FLIPPERONE_TOOLCHAIN_ROOT", raising=False)
    assert swd.find_openocd()[0] == pico
    monkeypatch.setenv("FLIPPERONE_TOOLCHAIN_ROOT", str(tmp_path / "bundle"))
    assert swd.find_openocd() == (bundle, str(tmp_path / "bundle" / "tools" / "openocd" / "scripts"))


def test_symbols_come_from_the_newest_elf_of_any_build_directory(tmp_path, monkeypatch):
    import os

    from agent_tools.logic import swd

    monkeypatch.setattr(swd, "REPO", str(tmp_path))
    assert swd.find_elf() is None
    old = tmp_path / "build" / "flipperone-mcu-firmware.elf"
    new = tmp_path / "build" / "f2-release" / "flipperone-mcu-firmware.elf"
    new.parent.mkdir(parents=True)
    old.write_text("")
    new.write_text("")
    os.utime(old, (1, 1))
    assert swd.find_elf() == str(new)


def test_outranking_slot_follows_the_bootrom_choice():
    from agent_tools.logic import flash

    def slots(a, b):
        return {"A": a, "B": b}

    def valid(major):
        return {"state": "valid", "version_major_word": major, "version_minor_word": 0}

    ours = {"version_major_word": 3, "version_minor_word": 0}
    plan_a, plan_b = {"slot": "A", "image": ours}, {"slot": "B", "image": ours}
    # A wins ties, so only a higher B outranks a build in A
    assert flash.outranking_slot(plan_a, slots(valid(1), valid(3))) is None
    assert flash.outranking_slot(plan_a, slots(valid(1), valid(4))) == "B"
    assert flash.outranking_slot(plan_b, slots(valid(3), valid(1))) == "A"
    assert flash.outranking_slot(plan_b, slots(valid(2), valid(1))) is None
    assert flash.outranking_slot(plan_b, slots({"state": "erased"}, valid(1))) is None
    assert flash.outranking_slot(plan_a, slots(valid(1), {"state": "no image header"})) is None


def test_write_partition_table_writes_and_verifies_at_the_start_of_flash(tmp_path, monkeypatch):
    from agent_tools import constants
    from agent_tools.logic import flash, swd

    commands, written = [], []

    def fake_run(lines, timeout=None):
        commands.append(lines)
        written.append(open(lines[1].split("{")[1].split("}")[0], "rb").read())

    monkeypatch.setattr(swd, "run", fake_run)
    table = tmp_path / "partition_table.bin"
    table.write_bytes(b"\x5a" * 148)
    flash.write_partition_table(str(table))
    # Both commands read one snapshot of the table, removed afterwards
    snapshot = commands[0][1].split("{")[1].split("}")[0]
    quoted = "{" + snapshot + "}"
    assert commands == [["reset halt", f"flash write_image erase {quoted} {constants.XIP_BASE:#x} bin",
                         f"verify_image {quoted} {constants.XIP_BASE:#x} bin"]]
    assert written == [b"\x5a" * 148] and snapshot != str(table)
    assert not Path(snapshot).exists()
