"""CLI contract: argument surface, JSON envelope, error handling without a device."""

import json
import sys

import pytest

from agent_tools.cli.main import build_parser, envelope, main


def run(monkeypatch, argv):
    monkeypatch.setattr(sys, "argv", ["f1.py"] + argv)
    return main()


def test_parser_surface():
    parser = build_parser()
    assert parser.parse_args(["ports"]).json is False
    assert parser.parse_args(["ports", "--json"]).json is True
    flash_args = parser.parse_args(["swd", "flash", "b", "fw.bin"])
    assert (flash_args.slot, flash_args.activate, flash_args.quiesce, flash_args.yes) == ("b", False, False, False)
    assert parser.parse_args(["swd", "flash", "a", "fw.bin", "--activate", "--quiesce", "--yes"]).activate is True
    read_args = parser.parse_args(["swd", "read", "0x10000000", "4", "--json"])
    assert (read_args.address, read_args.count, read_args.json) == ("0x10000000", 4, True)
    assert parser.parse_args(["touch", "512", "384", "--to", "600", "400"]).to == [600, 400]
    # Flash and touch never print JSON: they must reject the flag rather than silently ignore it.
    for argv in (["touch", "1", "2", "--json"], ["swd", "flash", "a", "fw.bin", "--json"]):
        with pytest.raises(SystemExit):
            parser.parse_args(argv)


def test_envelope_shape():
    args = build_parser().parse_args(["swd", "read", "0", "--json"])
    assert envelope(args, data={"words": []}) == {"tool": "f1.py", "command": "swd read", "ok": True, "data": {"words": []}}
    assert envelope(args, error="boom") == {"tool": "f1.py", "command": "swd read", "ok": False, "error": "boom"}


def test_invalid_pressure_fails_before_the_device(monkeypatch, capsys):
    assert run(monkeypatch, ["touch", "10", "10", "--pressure", "99999"]) == 1
    assert "Pressure 99999 is outside 0.." in capsys.readouterr().err


def test_json_error_goes_to_stderr(monkeypatch, capsys):
    assert run(monkeypatch, ["swd", "read", "notanumber", "--json"]) == 1
    captured = capsys.readouterr()
    assert captured.out == ""
    body = json.loads(captured.err)
    assert body["ok"] is False and body["command"] == "swd read"
    assert "notanumber" in body["error"]


def test_flash_refuses_without_confirmation(monkeypatch, capsys, tmp_path):
    image = tmp_path / "fw.bin"
    image.write_bytes(b"\x00" * 4096)
    assert run(monkeypatch, ["swd", "flash", "a", str(image)]) == 1
    assert "pass --yes" in capsys.readouterr().err


def test_unknown_command_exits_through_argparse(monkeypatch):
    monkeypatch.setattr(sys, "argv", ["f1.py", "nope"])
    with pytest.raises(SystemExit) as exit_info:
        build_parser().parse_args()
    assert exit_info.value.code == 2
