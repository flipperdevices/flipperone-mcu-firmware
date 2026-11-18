"""The Linux serial console: framed commands, prompt detection, clean poweroff."""

import time

from ..packets import host_shell, mcu_console
from ..transport import ports
from ..transport.serialport import SerialPort


class LinuxHost:
    """One Linux console port per command, opened on first use."""

    def __init__(self, open_port=None):
        self._open_port = open_port or (lambda: SerialPort(ports.require("linux"), ports.baud("linux")))
        self._port = None

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def close(self):
        if self._port is not None:
            self._port.close()
            self._port = None

    def port(self):
        if self._port is None:
            self._port = self._open_port()
        return self._port

    def ready(self):
        """(shell prompt seen, any output seen) on the Linux console."""
        port = self.port()
        any_output = False
        for _ in range(3):  # the first Enter can land in the middle of other output
            port.read_until_quiet(0.3, quiet=0.2, wait_for_data=False)
            port.write(b"\x03\r")
            banner = mcu_console.ANSI.sub(b"", port.read_until_quiet(2.0, quiet=0.4))
            banner = host_shell.strip_probe_banner(banner)
            any_output = any_output or bool(banner)
            if host_shell.SHELL_PROMPT.search(banner):
                return True, True
            if not banner:
                break  # a silent console stays silent; do not spend more time on it
        return False, any_output

    def state(self):
        """(shell answers, console shows any output)"""
        return self.ready()

    def exec(self, command, timeout=30.0, wait=0.0):
        """Run a shell command over the serial console. Returns (output, exit code)."""
        port = self.port()
        deadline = time.monotonic() + wait
        while True:
            ready, any_output = self.ready()
            if ready:
                break
            if time.monotonic() >= deadline:
                state = "shows no shell prompt: the host is still booting or at a login prompt" if any_output else "is silent: the host is off or waiting in the boot menu"
                raise RuntimeError(f"Linux console {state}. Check 'f1.py linux state' and 'f1.py screenshot', or retry with --wait SECONDS")
            time.sleep(2)
        port.write(f"stty -echo cols 250; {command}; echo {host_shell.DONE_PREFIX}$?; stty echo\r".encode())
        try:
            reply = port.read_until(lambda d: host_shell.DONE_PATTERN.search(d) is not None, timeout)
        except TimeoutError as error:
            raise TimeoutError(f"Command did not finish in {timeout:.0f}s. Output so far:\n{mcu_console.clean(error.data)}") from None
        port.read_until_quiet(0.5, quiet=0.2, wait_for_data=False)
        body_bytes, code = host_shell.split_done(reply)
        body = mcu_console.clean(body_bytes)
        first, _, rest = body.partition("\n")
        if "stty -echo" in first:  # the echo of the command line itself
            body = rest
        return body.strip("\n"), code

    def poweroff(self, timeout):
        """Ask Linux to power off and wait for the kernel to say it did.

        Nothing is typed into the console while waiting: a Ctrl+C there would
        interrupt the very command that is shutting the system down.
        """
        port = self.port()
        if not self.ready()[0]:
            raise RuntimeError("Linux console shows no shell prompt, a clean shutdown is not possible; use 'f1.py linux stop --hard'")
        deadline = time.monotonic() + timeout
        under_way = False
        while True:
            if not under_way:
                port.write(b"sync; sudo -n poweroff\r")
            try:
                port.read_until(lambda d: b"Power down" in d, min(30.0, max(deadline - time.monotonic(), 1.0)))
                return
            except TimeoutError as error:
                if time.monotonic() >= deadline:
                    raise RuntimeError(f"Linux did not power down in {timeout:.0f}s; use 'f1.py linux stop --hard'. Last console output:\n{mcu_console.clean(error.data)[-300:]}") from None
                # systemd ignores a poweroff issued while the system is still
                # starting up, so it is sent again unless a shutdown has begun
                under_way = under_way or b"power off now" in error.data
