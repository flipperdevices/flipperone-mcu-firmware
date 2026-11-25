#!/usr/bin/env python3
"""Log the battery, system-rail and input figures of a Flipper One to a CSV.

    python3 scripts/power_log.py OUT.csv                # over the probe's MCU console
    python3 scripts/power_log.py OUT.csv --period 0.5   # faster
    python3 scripts/power_log.py OUT.csv --console mcu-usb

Each row: host time, uptime of the log, charge state, battery level, battery
voltage and current (the gauge; current is negative on discharge, updated
once a second), the battery power it gives or takes, the system rail's
voltage, current and power (INA219, fast), input voltage and current. It runs `power
info` on the MCU console and parses the answer, so it works with Linux
running when the probe is attached, but mind that the charger, the gauge and
the INA219 sit on a bus shared with the host: sampling while Linux runs
collides with its own readings (it loses its power figures) and yields the
odd garbage row, which the plot should drop. Ctrl-C stops it; the CSV is
flushed after every row, so a plot can be made while it still runs.
"""

import argparse
import csv
import datetime
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from agent_tools.transport import ports  # noqa: E402
from agent_tools.transport.serialport import SerialPort  # noqa: E402

PROMPT = b"control>: "
ANSI = re.compile(rb"\x1b\[[0-9;]*[A-Za-z]")
FIELDS = ["time", "t_s", "state", "level_pct", "bat_mv", "bat_ma", "bat_w", "sys_mv", "sys_ma", "sys_w", "usb_mv", "usb_ma"]


def run_command(port, command, timeout):
    port.write(command.encode() + b"\r\n")
    deadline = time.time() + timeout
    out = b""
    while time.time() < deadline:
        chunk = port.read(0.05)
        if chunk:
            out += chunk
            if out.rstrip().endswith(PROMPT.rstrip()):
                break
    return ANSI.sub(b"", out).decode("latin1")


def parse_power_info(text):
    values = {}
    for line in text.splitlines():
        if ":" not in line:
            continue
        key, _, value = line.partition(":")
        key = key.strip()
        number = re.search(r"-?\d+(?:\.\d+)?", value)
        if number:
            values[key] = float(number.group())
        elif key == "state":
            values[key] = value.strip()
    return values


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("out", help="CSV file to append to")
    parser.add_argument("--period", type=float, default=0.1, help="seconds between samples (default 0.1; the gauge itself updates once a second, the INA219 faster)")
    parser.add_argument("--console", choices=["mcu", "mcu-usb"], default="mcu", help="MCU console to use (default: mcu, the UART one)")
    args = parser.parse_args(argv)

    path = ports.require(args.console)
    port = SerialPort(path, ports.baud(args.console))
    port.write(b"\r\n")
    port.read(0.3)

    new_file = not os.path.exists(args.out) or os.path.getsize(args.out) == 0
    started = time.time()
    with open(args.out, "a", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=FIELDS)
        if new_file:
            writer.writeheader()
        print(f"logging to {args.out} every {args.period} s from {path}, Ctrl-C to stop", file=sys.stderr)
        try:
            while True:
                tick = time.time()
                values = parse_power_info(run_command(port, "power info", timeout=2.0))
                if "BAT.voltage" in values and "BAT.current" in values:
                    bat_mv = values["BAT.voltage"]
                    bat_ma = values["BAT.current"]
                    row = {
                        "time": datetime.datetime.now().isoformat(timespec="milliseconds"),
                        "t_s": round(tick - started, 2),
                        "state": values.get("state", ""),
                        "level_pct": int(values.get("BAT.level", 0)),
                        "bat_mv": int(bat_mv),
                        "bat_ma": int(bat_ma),
                        "bat_w": round(bat_mv * bat_ma / 1e6, 3),
                        "sys_mv": int(values.get("SYS.voltage", 0)),
                        "sys_ma": round(values.get("SYS.current", 0.0), 1),
                        "sys_w": round(values.get("SYS.power", 0.0) / 1000.0, 3),
                        "usb_mv": int(values.get("USB.voltage", 0)),
                        "usb_ma": int(values.get("USB.current", 0)),
                    }
                    writer.writerow(row)
                    f.flush()
                    print(
                        f"{row['t_s']:8.1f}s  {row['state']:<12} {row['level_pct']:3d}%  bat {row['bat_mv']} mV {row['bat_ma']:5d} mA {row['bat_w']:7.3f} W   sys {row['sys_ma']:7.1f} mA {row['sys_w']:6.3f} W",
                        file=sys.stderr)
                else:
                    print("no answer from the console, retrying", file=sys.stderr)
                time.sleep(max(0.0, args.period - (time.time() - tick)))
        except KeyboardInterrupt:
            pass
        finally:
            port.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
