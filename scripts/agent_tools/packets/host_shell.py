"""Linux serial console text: prompt, probe banner, framed command status, uptime."""

import re

SHELL_PROMPT = re.compile(rb"[$#] ?$")
PROBE_BANNER = b"fixed baud rate"  # the probe prints its own line every time the port is opened
DONE_PREFIX = "__F1_DONE__"
DONE_PATTERN = re.compile(DONE_PREFIX.encode() + rb"(\d+)\r?\n")
UPTIME_LINE = re.compile(r"^\d+\.\d+$", re.M)


def strip_probe_banner(banner):
    """Drop the probe's own banner lines and surrounding blank space."""
    return b"\n".join(line for line in banner.split(b"\n") if PROBE_BANNER not in line).strip()


def split_done(reply):
    """Body and exit code from a reply that ends with the __F1_DONE__<code> marker."""
    match = DONE_PATTERN.search(reply)
    return reply[: match.start()], int(match.group(1))


def parse_uptime(output):
    """Seconds from the output of `cut -d' ' -f1 /proc/uptime`; kernel messages may be mixed into it."""
    numbers = UPTIME_LINE.findall(output)
    if not numbers:
        raise RuntimeError(f"Could not read the host uptime from: {output[-200:]}")
    return float(numbers[-1])
