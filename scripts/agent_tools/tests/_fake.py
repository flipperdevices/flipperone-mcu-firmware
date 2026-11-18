"""Fake serial port for tests: scripted responses to writes, buffer for reads."""

import time


class FakePort:
    """Duck-typed stand-in for transport.serialport.SerialPort.

    `responder(port, data)` is called on every write and may `feed()` bytes
    that the next read returns.
    """

    def __init__(self, responder=None):
        self.responder = responder
        self.written = []
        self.pending = b""
        self.closed = False

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def close(self):
        self.closed = True

    def write(self, data):
        data = bytes(data)
        self.written.append(data)
        if self.responder:
            self.responder(self, data)

    def feed(self, data):
        self.pending += data

    def read(self, timeout):
        data, self.pending = self.pending, b""
        return data

    def unread(self, data):
        self.pending = bytes(data) + self.pending

    def read_exact(self, count, timeout):
        deadline = time.monotonic() + timeout
        while len(self.pending) < count:
            if time.monotonic() > deadline:
                self.pending = b""
                raise TimeoutError(f"Got fewer than {count} bytes in {timeout:.1f}s")
            time.sleep(0.001)
        data, self.pending = self.pending[:count], self.pending[count:]
        return data

    def read_until_quiet(self, timeout, quiet=0.4, wait_for_data=True):
        data, self.pending = self.pending, b""
        return data

    def read_until(self, predicate, timeout):
        deadline = time.monotonic() + timeout
        while not predicate(self.pending):
            if time.monotonic() > deadline:
                error = TimeoutError(f"No expected reply in {timeout:.1f}s")
                error.data = self.pending
                self.pending = b""
                raise error
            time.sleep(0.001)
        data, self.pending = self.pending, b""
        return data


def console_responder(port, data):
    """MCU console: prompt after Ctrl-C/Enter, echo with output for commands, RPC ready."""
    if data.startswith(b"rpc"):
        port.feed(bytes([0xFD]))
    elif b"\x03" in data:
        port.feed(b"\r\ncontrol>: ")
    elif data == b"\r":
        port.feed(b"\r\ncontrol>: ")
    elif data.endswith(b"\r"):
        port.feed(data + b"\r\nreply\r\ncontrol>: ")


def linux_responder(port, data):
    """Linux console: shell prompt after Ctrl-C/Enter, framed output for stty lines."""
    if b"\x03" in data:
        port.feed(b"\r\nuser@flipper:~$ ")
    elif data.startswith(b"stty") and b"__F1_DONE__" in data:
        port.feed(b"\r\nhello\r\n__F1_DONE__0\r\n")
    elif data == b"\r":
        port.feed(b"user@flipper:~$ ")
