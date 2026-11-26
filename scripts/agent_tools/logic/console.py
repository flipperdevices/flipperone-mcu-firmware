"""MCU console session: sync and one-shot commands over a serial port."""

import re
import time

from ..packets import rpc
from ..packets.mcu_console import PROMPT, at_prompt, clean


# A command waiting for the user: its output ends with a y/n question
QUESTION = re.compile(rb"y/n\)?\s*$")


class McuConsole:
    def __init__(self, port):
        self.port = port

    def sync(self):
        """Get the console to an idle prompt, whatever state it was left in.

        The RPC close request goes first: inside a leftover RPC session plain
        text would be read as a message length. Ctrl+C clears the line.
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

    def send_raw(self, data, timeout=5.0):
        """Write bytes as they are, wait for no prompt; return what came back until 0.5 s of silence."""
        self.port.write(data)
        return clean(self.port.read_until_quiet(timeout, quiet=0.5, wait_for_data=False)).strip("\n")

    def command(self, line, timeout=5.0, answer=None):
        """Run one CLI command and return its output without the echo and the prompt.

        With answer, a y/n question the command asks is answered with it.
        """
        self.port.write(line.encode() + b"\r")
        done = lambda d: at_prompt(d) and line.encode() in d  # noqa: E731
        asked = lambda d: answer is not None and bool(QUESTION.search(clean(d).encode()))  # noqa: E731
        try:
            reply = self.port.read_until(lambda d: done(d) or asked(d), timeout)
            if not done(reply):
                self.port.write(answer.encode())
                reply += self.port.read_until(lambda d: at_prompt(d), timeout)
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
