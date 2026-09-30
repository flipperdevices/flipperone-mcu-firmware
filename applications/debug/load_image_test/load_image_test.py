"""
Load Image Test - sends an image to the device over serial (COM10) for the
"load_image_test" CLI command to display.

All image processing (loading, resizing, grayscale conversion, quantizing to
a limited number of gray levels) happens here; the device only parses the
hex string it receives and blits the resulting bytes straight to the screen.

The input image can be PNG, BMP, JPEG, or any other format Pillow's
Image.open() recognizes (it sniffs the file's actual content, not its
extension) - it just needs to be readable as a still image.

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
BAUD = 1500000
TIMEOUT = 2.0

DISPLAY_WIDTH = 258
DISPLAY_HEIGHT = 144
GRAY_LEVELS = 64

# cli_status.h's CLI_STATUS_OK ("RET: 0\r\n") / CLI_STATUS_ERROR ("RET: 1\r\n"):
# the command prints exactly one of them as its last line, whether it displayed
# the frame or reported why it didn't.
CLI_STATUS_OK_MARKER = "RET: 0"
CLI_STATUS_MARKER = "RET: "

# How long to wait for that status line once the frame has been sent. It has to
# outlast the transfer itself (the device only gives up on a silent sender after
# LOAD_IMAGE_TEST_STALL_TIMEOUT_MS = 5s, and a frame arrives in well under a
# second over USB), because a *fixed* read window - this used to be
# `read_for(ser, 1.0)` - raced exactly that transfer: a frame that took longer
# than the window to arrive was reported as a failure, and closing the port in
# response then cleared the picture it had just displayed.
STATUS_TIMEOUT = 10.0


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


def read_until_status(ser, timeout=STATUS_TIMEOUT):
    """Reads until the device's CLI status line ("RET: 0" / "RET: 1", see
    cli_status.h) has arrived, or until `timeout` runs out. That line, not a
    fixed read window, is what says the frame is done with. Returns everything
    that came back, decoded."""
    deadline = time.time() + timeout
    marker = CLI_STATUS_MARKER.encode("ascii")
    buf = b""
    while time.time() < deadline:
        chunk = ser.read(ser.in_waiting or 1)
        if chunk:
            buf += chunk
            if marker in buf:
                break
    return buf.decode(errors="replace")


def send_image(ser, image_path, requested_width, requested_height, levels):
    """Sends the command, resolves the frame size (an explicit `requested_width`/
    `requested_height` wins; otherwise the device's own banner is trusted;
    otherwise DISPLAY_WIDTH/DISPLAY_HEIGHT), then builds and sends the frame.
    Returns True if the device confirmed it displayed the frame, False
    otherwise."""
    # the interactive CLI shell submits on CR, not LF
    ser.write(b"load_image_test\r")
    banner = read_until(ser, b"> ")
    print(banner, end="")  # banner text, up to the command's one prompt

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
    response = read_until_status(ser)
    print(response, end="")
    return CLI_STATUS_OK_MARKER in response


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
    args = parser.parse_args()

    if args.levels < 2:
        parser.error("--levels must be at least 2")
    if args.width is not None and args.width <= 0:
        parser.error("--width must be positive")
    if args.height is not None and args.height <= 0:
        parser.error("--height must be positive")

    with serial.Serial(args.port, args.baud, timeout=0.1) as ser:
        ser.reset_input_buffer()

        ok = send_image(ser, args.image, args.width, args.height, args.levels)
        if not ok:
            print("Failed to display image.", file=sys.stderr)
            return 1

        # Closing this handle drops DTR, which the device reads as the
        # session going away (pipe_state() becomes PipeStateBroken) and
        # stops "load_image_test" - which then clears the frame. So the
        # image only stays up for as long as this script (and thus the
        # serial connection) is alive; keep it open until the user says
        # otherwise instead of exiting right after the send.
        print("Image sent. Press CTRL+C here to stop and clear the display.")
        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            pass
        return 0


if __name__ == "__main__":
    sys.exit(main())
