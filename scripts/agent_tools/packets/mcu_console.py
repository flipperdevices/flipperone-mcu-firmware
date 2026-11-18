"""MCU console text: prompt detection and ANSI cleanup."""

import re

PROMPT = re.compile(rb"\w+>: ?$")
ANSI = re.compile(rb"\x1b\[[0-9;?]*[a-zA-Z]")


def at_prompt(data):
    """True when the console output ends with an idle prompt."""
    return bool(PROMPT.search(ANSI.sub(b"", data[-64:]).rstrip(b"\r\n")))


def clean(data):
    """Console bytes to text: drop ANSI escapes and carriage returns."""
    return ANSI.sub(b"", data).replace(b"\r", b"").decode("utf-8", "replace")
