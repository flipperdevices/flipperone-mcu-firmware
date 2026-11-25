#!/usr/bin/env python3
"""Raw terminal to a Flipper One serial port, at any baud rate.

    python3 scripts/mcu_term.py mcu                   # a role from get_port.py
    python3 scripts/mcu_term.py /dev/cu.usbmodemX 1500000
    python3 scripts/mcu_term.py mcu --send "power show"

Keys go to the port as typed, the port's output goes to the screen, so the
console's own live screens (`power show`, `top`) just work. Ctrl-] quits.
`--send` types a command right after connecting. Unlike `screen`, it handles
the probe's 1500000 baud on macOS.
"""

import argparse
import os
import select
import sys
import termios
import tty

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from agent_tools import constants  # noqa: E402
from agent_tools.transport import ports  # noqa: E402
from agent_tools.transport.serialport import SerialPort  # noqa: E402

QUIT = b"\x1d"  # Ctrl-]


def resolve(target, baud):
    if target in constants.ROLES:
        path = ports.require(target)
        return path, baud or ports.baud(target)
    return target, baud or 115200


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("target", help="a role (mcu, mcu-usb, linux, probe, mcu-log) or a device path")
    parser.add_argument("baud", nargs="?", type=int, help="baud rate for a device path (default 115200)")
    parser.add_argument("--send", help="a command to type right after connecting")
    args = parser.parse_args(argv)

    path, baud = resolve(args.target, args.baud)
    port = SerialPort(path, baud)
    port_fd = port._impl.fd  # the raw descriptor, to wait on stdin and the port together
    print(f"{path} @ {baud}, Ctrl-] to quit", file=sys.stderr)

    stdin = sys.stdin.fileno()
    saved = termios.tcgetattr(stdin)
    tty.setraw(stdin)
    try:
        port.write(b"\r\n")
        if args.send:
            port.write(args.send.encode() + b"\r\n")
        while True:
            readable, _, _ = select.select([stdin, port_fd], [], [], 0.1)
            if stdin in readable:
                key = os.read(stdin, 64)
                if QUIT in key:
                    break
                port.write(key)
            if port_fd in readable:
                try:
                    data = os.read(port_fd, 4096)
                except BlockingIOError:
                    data = b""
                if data:
                    os.write(sys.stdout.fileno(), data)
    finally:
        termios.tcsetattr(stdin, termios.TCSADRAIN, saved)
        port.close()
        print(file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
