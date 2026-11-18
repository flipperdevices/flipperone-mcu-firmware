"""MCU console session: sync and one-shot commands over a serial port."""

import time

from ..packets import rpc
from ..packets.mcu_console import PROMPT, at_prompt, clean


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
        self.port.write(rpc.varint(2) + rpc.field_bytes(rpc.FIELD_SESSION_CLOSE, b""))
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
