"""Minimal non-interactive serial port access, standard library only on macOS and Linux.

Windows needs pyserial (`pip install pyserial`); that path is untested.
"""

import os
import sys
import time

_IOSSIOSPEED = 0x80045402  # macOS: set an arbitrary baud rate


class PortBusyError(RuntimeError):
    pass


class _PosixPort:
    def __init__(self, path, baud):
        import fcntl
        import struct
        import termios

        self.fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        try:
            fcntl.flock(self.fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError as exc:
            os.close(self.fd)
            raise PortBusyError(f"{path} is in use by another process") from exc

        attrs = termios.tcgetattr(self.fd)
        attrs[0] = attrs[1] = attrs[3] = 0  # raw: no input/output/local processing
        attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
        attrs[6][termios.VMIN] = 0
        attrs[6][termios.VTIME] = 0
        if sys.platform == "darwin":
            termios.tcsetattr(self.fd, termios.TCSANOW, attrs)
            fcntl.ioctl(self.fd, _IOSSIOSPEED, struct.pack("L", baud))
        else:
            speed = getattr(termios, f"B{baud}", None)
            if speed is None:
                raise ValueError(f"Baud rate {baud} is not supported by termios on this system")
            attrs[4] = attrs[5] = speed
            termios.tcsetattr(self.fd, termios.TCSANOW, attrs)

    def write(self, data):
        import select

        view = memoryview(data)
        while view:
            select.select([], [self.fd], [], 1.0)
            try:
                written = os.write(self.fd, view)
            except BlockingIOError:
                continue
            view = view[written:]

    def read(self, timeout):
        import select

        if not select.select([self.fd], [], [], max(timeout, 0))[0]:
            return b""
        try:
            return os.read(self.fd, 65536)
        except BlockingIOError:
            return b""

    def close(self):
        os.close(self.fd)


class _PySerialPort:
    def __init__(self, path, baud):
        import serial  # type: ignore

        self.port = serial.Serial(path, baud, timeout=0)

    def write(self, data):
        self.port.write(data)
        self.port.flush()

    def read(self, timeout):
        self.port.timeout = max(timeout, 0)
        data = self.port.read(1)
        if data:
            data += self.port.read(self.port.in_waiting or 0)
        return data

    def close(self):
        self.port.close()


class SerialPort:
    """Serial port with timeouts on every read. Use as a context manager."""

    def __init__(self, path, baud):
        self.path = path
        self._impl = _PySerialPort(path, baud) if os.name == "nt" else _PosixPort(path, baud)
        self._buffer = b""

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def close(self):
        self._impl.close()

    def write(self, data):
        self._impl.write(bytes(data))

    def read(self, timeout):
        """Return whatever arrives within `timeout` seconds; may be empty."""
        if self._buffer:
            data, self._buffer = self._buffer, b""
            return data
        return self._impl.read(timeout)

    def unread(self, data):
        self._buffer = bytes(data) + self._buffer

    def read_exact(self, count, timeout):
        """Return exactly `count` bytes or raise TimeoutError."""
        deadline = time.monotonic() + timeout
        data = b""
        while len(data) < count:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                self.unread(data)
                raise TimeoutError(f"Got {len(data)} of {count} bytes in {timeout:.1f}s")
            data += self.read(remaining)
        self.unread(data[count:])
        return data[:count]

    def read_until_quiet(self, timeout, quiet=0.4, wait_for_data=True):
        """Read until the line stays silent for `quiet` seconds or `timeout` expires.

        With wait_for_data the silence is counted from the first byte, so a
        slow reply is not missed; without it, from the call, so draining an
        idle line returns after `quiet`.
        """
        deadline = time.monotonic() + timeout
        data = b""
        last_data = time.monotonic()
        while time.monotonic() < deadline:
            chunk = self.read(0.02)
            if chunk:
                data += chunk
                last_data = time.monotonic()
            elif (data or not wait_for_data) and time.monotonic() - last_data >= quiet:
                break
        return data

    def read_until(self, predicate, timeout):
        """Read until predicate(data) is true; raise TimeoutError with what was read otherwise."""
        deadline = time.monotonic() + timeout
        data = b""
        while not predicate(data):
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                error = TimeoutError(f"No expected reply in {timeout:.1f}s")
                error.data = data
                raise error
            data += self.read(min(remaining, 0.1))
        return data
