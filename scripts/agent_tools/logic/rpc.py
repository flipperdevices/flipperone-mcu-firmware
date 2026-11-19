"""RPC session over the MCU console: screen capture, buttons, touch; PNG output."""

import struct
import time
import zlib

from ..packets import rpc
from ..packets.mcu_console import at_prompt, clean
from .console import McuConsole


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
            self.port.read_until(lambda d: bytes([rpc.RPC_READY]) in d, 5.0)
        except TimeoutError as error:
            raise RuntimeError(f"MCU did not enter RPC mode. Got: {clean(error.data)!r}") from None
        return self

    def __exit__(self, *exc):
        try:
            self._send(rpc.FIELD_SESSION_CLOSE, b"")
            # The MCU notices the close within 50 ms. Drain a frame that may be
            # in flight, then ask the shell for a prompt: it prints none by itself.
            self.port.read_until_quiet(2.0, quiet=0.2, wait_for_data=False)
            self.port.write(b"\r")
            try:
                self.port.read_until(at_prompt, 1.5)
            except TimeoutError:
                self.console.sync()
        except OSError:
            # The port went away with the MCU's USB console; nothing to close
            self.port_lost = True
        except Exception:
            if exc[0] is None:
                raise

    def _send(self, field, payload):
        message = rpc.field_bytes(field, payload)
        self.port.write(rpc.varint(len(message)) + message)

    def _receive(self, timeout):
        deadline = time.monotonic() + timeout
        size = shift = 0
        while True:
            byte = self.port.read_exact(1, max(deadline - time.monotonic(), 0.01))[0]
            size |= (byte & 0x7F) << shift
            shift += 7
            if not byte & 0x80:
                break
        return rpc.parse_message(self.port.read_exact(size, max(deadline - time.monotonic(), 0.01)))

    def screenshot(self, timeout=15.0):
        """(width, height, pixels): 8-bit grayscale, row by row.

        The MCU redraws when the stream starts, so the frame is current.
        """
        self._send(rpc.FIELD_START_VIRTUAL_DISPLAY, b"")
        try:
            deadline = time.monotonic() + timeout
            while True:
                message = self._receive(max(deadline - time.monotonic(), 0.01))
                if rpc.FIELD_FRAME in message:
                    break
        finally:
            self._send(rpc.FIELD_STOP_VIRTUAL_DISPLAY, b"")
        frame = rpc.parse_message(message[rpc.FIELD_FRAME])
        width, height, encoding, data = frame.get(1, 0), frame.get(2, 0), frame.get(3, 0), frame.get(5, b"")
        if encoding != 0:
            raise RuntimeError(f"Frame encoding {encoding} is not supported by this tool")
        if len(data) != width * height:
            raise RuntimeError(f"Frame is {len(data)} bytes, expected {width}x{height}")
        return width, height, data

    def button(self, button, action):
        payload = rpc.field_varint(1, rpc.BUTTONS[button]) + rpc.field_varint(2, rpc.ACTIONS[action])
        self._send(rpc.FIELD_BUTTON_EVENT, payload)

    def click(self, button, hold=0.0, repeats=0, gap=0.05):
        """Send the events the input service sends for a real button.

        Injected events derive nothing, so press, short/long, repeat and release
        are each sent explicitly.
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
            rpc.field_varint(1, rpc.TOUCH_TYPES[touch_type])
            + rpc.field_varint(2, x)
            + rpc.field_varint(3, y)
            + rpc.field_varint(4, pressure)
        )
        self._send(rpc.FIELD_TOUCH_EVENT, payload)


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
