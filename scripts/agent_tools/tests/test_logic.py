"""Logic layer against fake ports: console, RPC, Linux host, device session, flashing."""

import struct

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


def test_board_config_inherits_the_sdk_target_and_defines_the_staging_proc():
    from agent_tools.logic import swd

    text = open(swd.CONFIG).read()
    assert "source [find target/rp2350.cfg]" in text
    assert "proc f1_stage_tbyb {base}" in text


def test_flash_stages_a_tbyb_image_through_the_board_config(tmp_path, monkeypatch):
    from agent_tools.logic import flash, swd
    from test_packets import image_def_bytes

    commands = []
    monkeypatch.setattr(swd, "run", lambda lines, timeout=None: commands.append(lines))
    tbyb = tmp_path / "tbyb.bin"
    tbyb.write_bytes(image_def_bytes(tbyb=True) + b"\x00" * 64)
    plan = flash.flash("a", str(tbyb), activate=True)
    assert plan["staged"] is True and plan["tbyb"] is True
    assert [line for line in commands[0] if line.startswith("f1_stage_tbyb")] == [f"f1_stage_tbyb {plan['address']:#x}"]
    assert not any(line.startswith(("mww", "targets")) for line in commands[0])

    plain = tmp_path / "plain.bin"
    plain.write_bytes(image_def_bytes(tbyb=False) + b"\x00" * 64)
    commands.clear()
    assert flash.flash("a", str(plain), activate=True)["staged"] is False
    assert not any(line.startswith("f1_stage_tbyb") for line in commands[0])
