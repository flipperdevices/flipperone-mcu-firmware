"""Device session: one console connection per command, synced once."""

from contextlib import contextmanager

from ..transport import ports
from ..transport.serialport import SerialPort
from .console import McuConsole
from .rpc import RpcSession


class DeviceSession:
    """One console port for the command's lifetime, synced on first use."""


    def __init__(self, role=None, open_port=None):
        self._role = role or ports.mcu_console_role()
        self._open_port = open_port or (lambda role: SerialPort(ports.require(role), ports.baud(role)))
        self._port = None
        self._console = None
        self.port_lost = False

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def close(self):
        if self._port is not None:
            self._port.close()
            self._port = None
            self._console = None

    def port(self):
        if self._port is None:
            self._port = self._open_port(self._role)
        return self._port

    def console(self):
        if self._console is None:
            self._console = McuConsole(self.port())
            self._console.sync()
        return self._console

    def command(self, line, timeout=5.0):
        return self.console().command(line, timeout)

    def app(self):
        """App id running on the MCU desktop, or None."""
        reply = self.command("desktop get_run_app")
        return reply.split(":", 1)[1].strip() if "running app:" in reply else None

    @contextmanager
    def rpc(self):
        """RPC session on the same port; keeps the session's port-lost flag up to date."""
        session = RpcSession(self.port())
        session.__enter__()
        try:
            yield session
        except BaseException as exc:
            try:
                session.__exit__(type(exc), exc, exc.__traceback__)
            finally:
                self.port_lost = self.port_lost or session.port_lost
            raise
        else:
            session.__exit__(None, None, None)
            self.port_lost = self.port_lost or session.port_lost
