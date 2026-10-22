"""
Load Image Test - sends an image to the device over serial (COM10) for the
"load_image_test" CLI command to display.

All image processing (loading, resizing, grayscale conversion, quantizing to
a limited number of gray levels) happens here; the device only parses the
hex string it receives and blits the resulting bytes straight to the screen.

The input image can be PNG, BMP, JPEG, or any other format Pillow's
Image.open() recognizes (it sniffs the file's actual content, not its
extension) - it just needs to be readable as a still image.

Works over both CLI transports: the USB CDC port (where asserting DTR is what
makes the device start a session) and a TTL UART adapter at 1500000 baud, 8N1
(cli_uart.c's UART_BAUD_RATE). What differs between them is how a session ends:
closing the USB port drops DTR and tears the whole session down, while a UART
session lives from boot to reboot - so the command is stopped with CTRL+C
instead of by closing the port, see send_ctrl_c().

Usage:
    python load_image_test.py test_image.bmp
    python load_image_test.py tv_test_pattern.png
    python load_image_test.py test_image.bmp --port COM5
"""

import argparse
import re
import sys
import time

import serial
from PIL import Image

PORT = "COM10"
# 1.5 Mbaud is what both CLI transports run at (cli_uart.c's UART_BAUD_RATE);
# the USB CDC port ignores the value entirely.
BAUD = 1500000
TIMEOUT = 2.0

# How long to let the device settle before typing at it: a USB connection only
# gets its CLI session when DTR is asserted, and a USB-TTL adapter with an
# auto-reset circuit (DTR wired to RESET) may have just rebooted it.
SETTLE_DELAY = 0.3

# CTRL+C, i.e. CliKeyETX - the one input load_image_test's own wait_for_stop()
# loop honours, see send_ctrl_c().
CTRL_C = b"\x03"

# The shell prints its prompt as "%s>: " (cli_shell_line_format_prompt()) while
# the command's banner ends with a bare "> ", so one cannot be mistaken for the
# other - read_until() below relies on that.
SHELL_PROMPT_MARKER = b">: "
COMMAND_BANNER_MARKER = "Load Image Test"

DISPLAY_WIDTH = 258
DISPLAY_HEIGHT = 144
GRAY_LEVELS = 64

# cli_status.h's CLI_STATUS_OK ("RET: 0\r\n") / CLI_STATUS_ERROR ("RET: 1\r\n"):
# the command prints exactly one of them as its last line, whether it displayed
# the frame or reported why it didn't.
CLI_STATUS_OK_MARKER = "RET: 0"
CLI_STATUS_MARKER = "RET: "

# How long to wait for that status line once the frame has been sent.
#
# It has to outlast the transfer, and *that* is transport-dependent: over USB a
# frame is in within well under a second, but over a TTL UART the firmware's
# 10ms poll of its receive pipe plus its ~32-byte UART RX batches stretch it to
# tens of seconds (measured: 12782ms for 74304 hex chars, i.e. ~5.8 KB/s). A
# shorter budget reported "Failed to display image." for frames that were still
# being received - and closing the port then threw away the "Displayed ...
# RET: 0" line the device sent moments later. Override with --wait if needed.
STATUS_WAIT_TIMEOUT = 60.0

# While waiting, print a heartbeat at most this often - a UART transfer takes
# long enough that silence would otherwise look like a hang.
STATUS_HEARTBEAT_S = 5.0


def to_display_buffer(path, width, height, levels):
    """Loads `path`, converts it to an 8-bit grayscale, `width`x`height`,
    `levels`-level image, and returns its raw row-major pixel bytes (top to
    bottom, left to right - one byte per pixel), matching the device's
    Canvas/Color layout."""
    img = Image.open(path).convert("L")
    if img.size != (width, height):
        img = img.resize((width, height))

    # quantize to `levels` evenly-spaced steps, still stored as full 8-bit values
    step = 255 / (levels - 1)
    lut = [round(round(v / step) * step) for v in range(256)]
    img = img.point(lut)

    return img.tobytes()


def parse_expected_size(banner: str):
    """Extracts the (width, height) the device itself expects out of the
    command's banner text (e.g. "Send a 258x144 8-bit grayscale frame..."),
    so the host doesn't have to keep its own copy of the panel's resolution
    in sync with the firmware's. Returns None if the banner doesn't match
    (unexpected reply, e.g. an older firmware build)."""
    match = re.search(r"Send a (\d+)x(\d+)", banner)
    if not match:
        return None
    return int(match.group(1)), int(match.group(2))


def read_until(ser, marker: bytes, timeout=TIMEOUT):
    """Reads and returns everything up to (and including) `marker`, or
    whatever arrived before `timeout` runs out."""
    deadline = time.time() + timeout
    buf = b""
    while time.time() < deadline:
        chunk = ser.read(ser.in_waiting or 1)
        if chunk:
            buf += chunk
            if buf.endswith(marker):
                break
    return buf.decode(errors="replace")


def read_until_status(ser, timeout=STATUS_WAIT_TIMEOUT):
    """Reads until the device's CLI status line ("RET: 0" / "RET: 1", see
    cli_status.h) has arrived, or until `timeout` runs out. That line, not a
    fixed read window, is what says the frame is done with - and since the
    device stays silent until the whole frame is in, the wait has to cover the
    transfer itself, not just the device's reaction to it. Returns everything
    that came back, decoded."""
    start = time.time()
    deadline = start + timeout
    marker = CLI_STATUS_MARKER.encode("ascii")
    buf = b""
    heartbeats = 0
    while time.time() < deadline:
        chunk = ser.read(ser.in_waiting or 1)
        if chunk:
            buf += chunk
            if marker in buf:
                break
            continue
        waited = time.time() - start
        if waited >= (heartbeats + 1) * STATUS_HEARTBEAT_S:
            heartbeats += 1
            print(f"  ... still waiting for the device ({waited:.0f}s)", flush=True)
    return buf.decode(errors="replace")


def open_serial(port=PORT, baud=BAUD, timeout=0.1):
    """Opens `port` and asserts DTR explicitly.

    DTR only means something to the USB CDC port: that is what makes the device
    start a CLI session for the connection (cli_vcp.c creates the shell on the
    DTR edge). A plain UART port ignores it - and that is exactly why closing
    this handle cannot be used to stop the command, see send_ctrl_c()."""
    ser = serial.Serial(port, baud, timeout=timeout)
    ser.dtr = True
    return ser


def send_ctrl_c(ser):
    """Interrupts whatever is reading the device's pipe.

    This is the only way to stop `load_image_test` on a UART: there, dropping
    DTR (i.e. closing the port) does not break the CLI session, whereas on USB it
    does - closing the handle tears the session down and the command notices via
    PipeStateBroken. Leaving the command running is not just cosmetic: its
    wait_for_stop() loop keeps consuming the pipe, so the next run's command line
    would be swallowed by it and never reach the shell."""
    ser.write(CTRL_C)
    ser.flush()


def sync_to_shell(ser):
    """Cancels any running command and waits for the shell's prompt again.

    Returns whatever the device said in the meantime (may be empty if the
    prompt never showed up, e.g. wrong port or baud rate - the caller decides
    whether that matters)."""
    send_ctrl_c(ser)
    return read_until(ser, SHELL_PROMPT_MARKER)


def send_image(ser, image_path, requested_width, requested_height, levels, wait=STATUS_WAIT_TIMEOUT):
    """Sends the command, resolves the frame size (an explicit `requested_width`/
    `requested_height` wins; otherwise the device's own banner is trusted;
    otherwise DISPLAY_WIDTH/DISPLAY_HEIGHT), then builds and sends the frame.
    Returns True if the device confirmed it displayed the frame, False
    otherwise."""
    # Get to a known state before typing at the device: let it settle, drain
    # whatever it is still saying, and cancel a command a previous run may have
    # left behind.
    time.sleep(SETTLE_DELAY)
    ser.reset_input_buffer()
    sync_to_shell(ser)

    # the interactive CLI shell submits on CR, not LF
    ser.write(b"load_image_test\r")
    banner = read_until(ser, b"> ")
    print(banner, end="")  # banner text, up to the command's one prompt

    if COMMAND_BANNER_MARKER not in banner:
        print(
            "error: the load_image_test command did not answer - wrong port, wrong"
            " baud rate, or firmware without that command. Sending nothing: a frame"
            " would just end up in the shell's input buffer.",
            file=sys.stderr)
        return False

    parsed_size = parse_expected_size(banner)
    width = requested_width if requested_width is not None else (parsed_size[0] if parsed_size else DISPLAY_WIDTH)
    height = requested_height if requested_height is not None else (parsed_size[1] if parsed_size else DISPLAY_HEIGHT)
    if parsed_size and (width, height) != parsed_size:
        print(f"warning: sending {width}x{height}, but the device expects {parsed_size[0]}x{parsed_size[1]}")

    data = to_display_buffer(image_path, width, height, levels)

    hex_str = data.hex()
    ser.write(hex_str.encode("ascii") + b"\r\n")
    # The command shows exactly one prompt (above) then either displays the
    # frame and waits for CTRL+C, or reports an error and exits - either way
    # there's no second prompt to wait for, just the one status line that ends
    # whichever of those happened.
    response = read_until_status(ser, wait)
    print(response, end="")
    if CLI_STATUS_OK_MARKER in response:
        return True

    print(
        f"hint: the device sent no status line within {wait:.0f}s. Over a TTL UART"
        " a frame takes tens of seconds to arrive - raise --wait if the device was"
        " still receiving it.",
        file=sys.stderr)
    return False


def main():
    parser = argparse.ArgumentParser(
        description="Send an image to the device's load_image_test CLI command for display")
    parser.add_argument("image", help="Path to the image to display (any format Pillow can read)")
    parser.add_argument("--port", default=PORT, help=f"Serial port (default: {PORT})")
    parser.add_argument("--baud", type=int, default=BAUD, help=f"Baud rate (default: {BAUD})")
    parser.add_argument(
        "--width",
        type=int,
        default=None,
        help=f"Display width in pixels (default: parsed from the device's own banner, else {DISPLAY_WIDTH})")
    parser.add_argument(
        "--height",
        type=int,
        default=None,
        help=f"Display height in pixels (default: parsed from the device's own banner, else {DISPLAY_HEIGHT})")
    parser.add_argument(
        "--levels",
        type=int,
        default=GRAY_LEVELS,
        help=f"Number of gray levels to quantize to, >= 2 (default: {GRAY_LEVELS})")
    parser.add_argument(
        "--wait",
        type=float,
        default=STATUS_WAIT_TIMEOUT,
        help=f"Seconds to wait for the device's status line after sending the frame"
             f" (default: {STATUS_WAIT_TIMEOUT:.0f}; a TTL UART needs far more time"
             " than USB)")
    args = parser.parse_args()

    if args.levels < 2:
        parser.error("--levels must be at least 2")
    if args.width is not None and args.width <= 0:
        parser.error("--width must be positive")
    if args.height is not None and args.height <= 0:
        parser.error("--height must be positive")

    with open_serial(args.port, args.baud) as ser:
        try:
            ok = send_image(
                ser, args.image, args.width, args.height, args.levels, wait=args.wait)
            if not ok:
                print("Failed to display image.", file=sys.stderr)
                return 1

            # The command keeps re-pushing the frame (its keepalive), so the
            # picture stays up until it is told to stop - press CTRL+C here when
            # done, instead of exiting right after the send.
            print("Image sent. Press CTRL+C here to stop and clear the display.")
            try:
                while True:
                    time.sleep(1)
            except KeyboardInterrupt:
                pass
            return 0
        finally:
            # Stop the command explicitly rather than relying on the port going
            # away - see send_ctrl_c(). Dropping DTR by closing the handle still
            # happens below and is harmless on both transports.
            try:
                send_ctrl_c(ser)
                # Give the adapter a moment to actually push that byte out
                # before the handle (and with it the line) goes away.
                time.sleep(0.1)
            except Exception:
                pass


if __name__ == "__main__":
    sys.exit(main())
