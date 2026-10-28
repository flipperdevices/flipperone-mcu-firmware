#!/usr/bin/env python3
"""MCU operations: device_info, post-flash sanity, and on-device unit-tests.

Like BSB's testops.py, transport, device operations and CLI exit status are
separate. This stand uses a leased MCU UART rather than BusyBar telnet.
"""

import argparse
from dataclasses import dataclass
import json
import os
from pathlib import Path
import re
import sys
import time


ANSI = re.compile(rb"\x1b\[[0-?]*[ -/]*[@-~]")
PROMPT = re.compile(rb"(?:^|\n)control>: *\Z")


class CommandTimeout(TimeoutError):
    def __init__(self, message, output):
        super().__init__(message)
        self.output = output


def normalized(raw):
    return ANSI.sub(b"", raw).replace(b"\r", b"")


def parse_result(raw):
    output = normalized(raw)
    statuses = re.findall(r"^Status: (PASSED|FAILED)\s*$", output.decode(errors="replace"), re.M)
    if statuses != ["PASSED"]:
        raise ValueError(f"MCU unit_tests did not pass: status={statuses}")
    if not PROMPT.search(output):
        raise ValueError("MCU did not return to the CLI prompt")
    return {"ok": True, "status": "PASSED"}


def read_prompt(port, timeout, transcript):
    deadline = time.monotonic() + timeout
    received = bytearray()
    while time.monotonic() < deadline:
        chunk = port.read(4096)
        if not chunk:
            continue
        transcript.write(chunk)
        transcript.flush()
        received.extend(chunk)
        if len(received) > 8 * 1024 * 1024:
            raise ValueError("MCU response exceeded 8 MiB")
        if PROMPT.search(normalized(received)):
            return bytes(received)
    raise CommandTimeout(f"MCU did not finish and return a prompt within {timeout}s", bytes(received))


@dataclass(frozen=True)
class SerialSettings:
    port: str
    baud: int = 1500000


@dataclass
class CommandResult:
    ok: bool
    command: str
    raw: bytes
    duration_sec: float
    error: str = ""

    @property
    def stdout(self):
        return normalized(self.raw).decode(errors="replace")


class SerialClient:
    def __init__(self, settings, transcript):
        self.settings = settings
        self.transcript = transcript
        self.port = None

    def __enter__(self):
        import serial
        self.port = serial.Serial(self.settings.port, self.settings.baud,
                                  timeout=0.1, write_timeout=3, exclusive=True)
        try:
            # Synchronize before sending a command. A stale prompt/summary
            # cannot establish completion of the command about to run.
            self.port.reset_input_buffer()
            self.port.write(b"\x03")
            read_prompt(self.port, 10, self.transcript)
        except Exception:
            self.port.close()
            raise
        return self

    def __exit__(self, *exc):
        self.port.close()

    def run(self, command, timeout):
        started = time.monotonic()
        try:
            self.port.reset_input_buffer()
            self.port.write(command.encode() + b"\r")
            raw = read_prompt(self.port, timeout, self.transcript)
        except (TimeoutError, OSError, ValueError) as error:
            return CommandResult(False, command, getattr(error, "output", b""),
                                 time.monotonic() - started, str(error))
        return CommandResult(True, command, raw, time.monotonic() - started)


class McuDevice:
    def __init__(self, settings, transcript):
        self.settings = settings
        self.transcript = transcript
        self.last_result = {"ok": False}

    def _serial(self):
        return SerialClient(self.settings, self.transcript)

    def get_device_info(self, timeout):
        with self._serial() as client:
            result = client.run("device_info", timeout)
        self.last_result = {"ok": result.ok, "command": "device_info"}
        if result.error:
            self.last_result["error"] = result.error
        return result.ok, result.stdout

    def sanity(self, commit, target, timeout):
        ok, output = self.get_device_info(timeout)
        if not ok:
            return False, output
        actual = dict(re.findall(r"^([\w.]+)\s*:\s*(.*?)\s*$", output, re.M))
        expected = {"rp2350_firmware_commit": commit[:8],
                    "rp2350_firmware_commit_dirty": "false",
                    "rp2350_firmware_target": target[1:]}
        mismatches = {key: {"expected": value, "actual": actual.get(key)}
                      for key, value in expected.items() if actual.get(key) != value}
        self.last_result = {"ok": not mismatches, "command": "sanity",
                            "versions": actual, "mismatches": mismatches}
        return not mismatches, output

    def run_unit_tests(self, timeout):
        with self._serial() as client:
            result = client.run("unit_tests", timeout)
        self.last_result = {"ok": False, "command": "unit_tests"}
        if not result.ok:
            self.last_result["error"] = result.error
            return False, result.stdout
        try:
            self.last_result.update(parse_result(result.raw))
        except ValueError as error:
            self.last_result["error"] = str(error)
            return False, result.stdout
        return True, result.stdout


class McuTestOps:
    def _make_device_from_args(self, args, transcript):
        if args.port:
            settings = SerialSettings(args.port, args.baud)
        else:
            lease = json.loads(args.lease.read_text())
            dut = next(d for d in lease["duts"] if d["name"] == args.dut)
            uart = dut["uarts"]["mcu-cli"]
            settings = SerialSettings(uart["path"], uart["baud"])
        return McuDevice(settings, transcript)

    def _execute(self, args):
        args.output.mkdir(parents=True, exist_ok=True)
        started = time.monotonic()
        result = {"ok": False, "command": args.operation}
        ok = False
        try:
            with (args.output / f"{args.operation}.raw.log").open("wb") as transcript:
                device = self._make_device_from_args(args, transcript)
                if args.operation == "unit-tests":
                    ok, output = device.run_unit_tests(args.timeout)
                elif args.operation == "sanity":
                    ok, output = device.sanity(args.commit, args.target, args.timeout)
                else:
                    ok, output = device.get_device_info(args.timeout)
                result.update(device.last_result)
                print(output if output else "(No output received)", flush=True)
                if result.get("error"):
                    print(result["error"], file=sys.stderr)
        except Exception as error:
            result["error"] = str(error)
            print(f"MCU {args.operation} failed: {error}", file=sys.stderr)
        finally:
            result["ok"] = ok
            result["seconds"] = round(time.monotonic() - started, 3)
            name = "result.json" if args.operation == "unit-tests" else f"{args.operation}.json"
            (args.output / name).write_text(json.dumps(result, indent=2) + "\n")
            summary = os.environ.get("GITHUB_STEP_SUMMARY")
            if summary and args.operation == "unit-tests":
                with open(summary, "a") as stream:
                    stream.write(f"MCU `unit_tests`: **{'PASSED' if ok else 'FAILED'}**\n")
        return 0 if ok else 1

    def run(self, argv=None):
        parser = argparse.ArgumentParser(description=__doc__)
        commands = parser.add_subparsers(dest="command", required=True)
        for name, aliases in (("unit-tests", ["unit_tests", "tests"]),
                              ("device_info", ["device-info", "info"]), ("sanity", [])):
            command = commands.add_parser(name, aliases=aliases)
            command.add_argument("--lease", type=Path,
                                 default=Path(os.environ.get("FO_LEASE", "/run/fo/lease.json")))
            command.add_argument("--dut", default="dut0")
            command.add_argument("--port", help="MCU UART; default: mcu-cli from FO_LEASE")
            command.add_argument("--baud", type=int, default=1500000)
            command.add_argument("-t", "--timeout", type=float, default=300 if name == "unit-tests" else 10)
            command.add_argument("--output", type=Path, default=Path("test-results"))
            if name == "sanity":
                command.add_argument("--commit", required=True)
                command.add_argument("--target", choices=["f1", "f2"], required=True)
            command.set_defaults(operation=name)
        return self._execute(parser.parse_args(argv))


if __name__ == "__main__":
    sys.exit(McuTestOps().run())
