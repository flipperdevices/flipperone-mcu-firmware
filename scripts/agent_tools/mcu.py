"""MCU console (CLI) and RPC session over it: screen capture, buttons, touch.

The RPC protocol is assets/proto/rpc.proto: after the `rpc` CLI command the
MCU answers with one 0xFD byte, then both sides exchange length-delimited
RpcMessage protobufs. The messages are simple enough to encode by hand, so
nothing here needs the protobuf package or generated code.
"""

import re
import time
import zlib
import struct

PROMPT = re.compile(rb"\w+>: ?$")
ANSI = re.compile(rb"\x1b\[[0-9;?]*[a-zA-Z]")

RPC_READY = 0xFD

# RpcMessage oneof field numbers
FIELD_FRAME = 1
FIELD_BUTTON_EVENT = 2
FIELD_TOUCH_EVENT = 3
FIELD_START_VIRTUAL_DISPLAY = 4
FIELD_STOP_VIRTUAL_DISPLAY = 5
FIELD_SESSION_CLOSE = 6

BUTTONS = {
    "ok": 0, "back": 1, "key1": 2, "key2": 3, "power": 4, "key4": 5, "key5": 6,
    "sw": 7, "down": 8, "right": 9, "left": 10, "up": 11, "ptt": 12,
}  # fmt: skip
ACTIONS = {"press": 0, "release": 1, "short": 2, "long": 3, "repeat": 4}
TOUCH_TYPES = {"start": 0, "move": 1, "end": 2}
# Touchpad coordinate range, from applications/services/input_touch/input_touch.h
TOUCH_MAX_X, TOUCH_MAX_Y, TOUCH_MAX_PRESSURE = 1023, 767, 1024 * 16 - 1


def at_prompt(data):
    """True when the console output ends with an idle prompt."""
    return bool(PROMPT.search(ANSI.sub(b"", data[-64:]).rstrip(b"\r\n")))


def clean(data):
    """Console bytes to text: drop ANSI escapes and carriage returns."""
    return ANSI.sub(b"", data).replace(b"\r", b"").decode("utf-8", "replace")


# --- protobuf wire format ---------------------------------------------------


def _varint(value):
    out = bytearray()
    while True:
        byte = value & 0x7F
        value >>= 7
        out.append(byte | (0x80 if value else 0))
        if not value:
            return bytes(out)


def _field_varint(number, value):
    # proto3 omits zero values; the decoder fills them in
    return _varint(number << 3) + _varint(value) if value else b""


def _field_bytes(number, payload):
    return _varint((number << 3) | 2) + _varint(len(payload)) + payload


def _parse(payload):
    """Decode one message level into {field number: int or bytes}."""
    fields, pos = {}, 0

    def varint():
        nonlocal pos
        value = shift = 0
        while True:
            byte = payload[pos]
            pos += 1
            value |= (byte & 0x7F) << shift
            shift += 7
            if not byte & 0x80:
                return value

    while pos < len(payload):
        key = varint()
        number, wire_type = key >> 3, key & 7
        if wire_type == 0:
            fields[number] = varint()
        elif wire_type == 2:
            size = varint()
            fields[number] = payload[pos : pos + size]
            pos += size
        else:
            raise ValueError(f"Unexpected protobuf wire type {wire_type}")
    return fields


# --- console ----------------------------------------------------------------


class McuConsole:
    def __init__(self, port):
        self.port = port

    def sync(self):
        """Get the console to an idle prompt, whatever state it was left in.

        A crashed tool can leave the console inside an RPC session, where plain
        text would be parsed as a message length and swallow what follows. So
        the RPC close request goes first: it ends a session cleanly and is
        harmless noise at a prompt. Ctrl+C then stops a running command or
        clears the line.
        """
        self.port.read_until_quiet(0.5, quiet=0.1, wait_for_data=False)
        self.port.write(_varint(2) + _field_bytes(FIELD_SESSION_CLOSE, b""))
        time.sleep(0.15)
        self.port.write(b"\x03")
        self.port.read_until_quiet(1.0, quiet=0.15, wait_for_data=False)
        self.port.write(b"\r")
        try:
            reply = self.port.read_until(at_prompt, 2.0)
        except TimeoutError as error:
            reply = error.data
        if not at_prompt(reply):
            raise RuntimeError(
                "MCU console does not show a prompt. It may be stuck or the MCU may be down; "
                f"check 'f1.py swd status'. Got: {clean(reply)[-200:]!r}"
            )

    def command(self, line, timeout=5.0):
        """Run one CLI command and return its output without the echo and the prompt."""
        self.port.write(line.encode() + b"\r")
        try:
            reply = self.port.read_until(lambda d: at_prompt(d) and line.encode() in d, timeout)
        except TimeoutError as error:
            # Leave the console usable for the next call
            self.port.write(b"\x03")
            self.port.read_until_quiet(1.0)
            raise TimeoutError(f"'{line}' did not finish in {timeout:.0f}s. Output so far:\n{clean(error.data)}") from None
        text = clean(reply)
        text = text.split(line, 1)[-1]  # drop the echo
        lines = text.strip("\n").split("\n")
        if lines and PROMPT.search(lines[-1].encode()):
            lines.pop()
        return "\n".join(lines).strip("\n")


# --- RPC --------------------------------------------------------------------


class RpcSession:
    """Use as a context manager so the session is always closed."""

    def __init__(self, port):
        self.port = port
        self.console = McuConsole(port)
        self.port_lost = False

    def __enter__(self):
        self.console.sync()
        self.port.write(b"rpc\r")
        try:
            self.port.read_until(lambda d: bytes([RPC_READY]) in d, 5.0)
        except TimeoutError as error:
            raise RuntimeError(f"MCU did not enter RPC mode. Got: {clean(error.data)!r}") from None
        return self

    def __exit__(self, *exc):
        try:
            self._send(FIELD_SESSION_CLOSE, b"")
            # The MCU notices the close within 50 ms. Drain a frame that may be
            # in flight, then ask the shell for a prompt: it prints none by itself.
            self.port.read_until_quiet(2.0, quiet=0.2, wait_for_data=False)
            self.port.write(b"\r")
            try:
                self.port.read_until(at_prompt, 1.5)
            except TimeoutError:
                self.console.sync()
        except OSError:
            # The port itself went away: the MCU's USB console is switched off
            # when Linux powers up. The session died with it; nothing to close.
            self.port_lost = True
        except Exception:
            if exc[0] is None:
                raise

    def _send(self, field, payload):
        message = _field_bytes(field, payload)
        self.port.write(_varint(len(message)) + message)

    def _receive(self, timeout):
        deadline = time.monotonic() + timeout
        size = shift = 0
        while True:
            byte = self.port.read_exact(1, max(deadline - time.monotonic(), 0.01))[0]
            size |= (byte & 0x7F) << shift
            shift += 7
            if not byte & 0x80:
                break
        return _parse(self.port.read_exact(size, max(deadline - time.monotonic(), 0.01)))

    def screenshot(self, timeout=15.0):
        """Return (width, height, pixels): 8-bit grayscale, one byte per pixel, row by row.

        Starting the stream makes the MCU redraw, so the first frame is the
        current screen and arrives right away even if nothing is changing.
        """
        self._send(FIELD_START_VIRTUAL_DISPLAY, b"")
        try:
            deadline = time.monotonic() + timeout
            while True:
                message = self._receive(max(deadline - time.monotonic(), 0.01))
                if FIELD_FRAME in message:
                    break
        finally:
            self._send(FIELD_STOP_VIRTUAL_DISPLAY, b"")
        frame = _parse(message[FIELD_FRAME])
        width, height, encoding, data = frame.get(1, 0), frame.get(2, 0), frame.get(3, 0), frame.get(5, b"")
        if encoding != 0:
            raise RuntimeError(f"Frame encoding {encoding} is not supported by this tool")
        if len(data) != width * height:
            raise RuntimeError(f"Frame is {len(data)} bytes, expected {width}x{height}")
        return width, height, data

    def button(self, button, action):
        payload = _field_varint(1, BUTTONS[button]) + _field_varint(2, ACTIONS[action])
        self._send(FIELD_BUTTON_EVENT, payload)

    def click(self, button, hold=0.0, repeats=0, gap=0.05):
        """Send what the input service sends for a real button.

        Short click: press, short, release. With `hold` seconds: press, then
        long, then `repeats` repeat events, then release. The device derives
        nothing from injected events, so every event is sent explicitly.
        """
        self.button(button, "press")
        if hold > 0:
            time.sleep(hold)
            self.button(button, "long")
            for _ in range(repeats):
                time.sleep(gap)
                self.button(button, "repeat")
            time.sleep(gap)
        else:
            time.sleep(gap)
            self.button(button, "short")
        self.button(button, "release")
        time.sleep(gap)

    def touch(self, touch_type, x, y, pressure=100):
        payload = (
            _field_varint(1, TOUCH_TYPES[touch_type]) + _field_varint(2, x) + _field_varint(3, y) + _field_varint(4, pressure)
        )
        self._send(FIELD_TOUCH_EVENT, payload)


# --- image ------------------------------------------------------------------


def write_png(path, width, height, pixels, scale=1):
    """Write 8-bit grayscale pixels as a PNG, enlarged `scale` times."""
    rows = []
    for y in range(height):
        row = pixels[y * width : (y + 1) * width]
        if scale > 1:
            row = bytes(value for value in row for _ in range(scale))
        rows.extend([b"\x00" + row] * scale)

    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))

    header = struct.pack(">IIBBBBB", width * scale, height * scale, 8, 0, 0, 0, 0)
    with open(path, "wb") as file:
        file.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(b"".join(rows), 9)) + chunk(b"IEND", b""))
