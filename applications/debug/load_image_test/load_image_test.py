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
import time

import serial
from PIL import Image

PORT = "COM10"
BAUD = 1500000
TIMEOUT = 2.0

DISPLAY_WIDTH = 258
DISPLAY_HEIGHT = 144
GRAY_LEVELS = 64


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


def read_for(ser, duration: float):
    """Reads and returns whatever arrives over `duration` seconds."""
    deadline = time.time() + duration
    buf = b""
    while time.time() < deadline:
        chunk = ser.read(ser.in_waiting or 1)
        if chunk:
            buf += chunk
    return buf.decode(errors="replace")


def send_image(ser, data):
    # the interactive CLI shell submits on CR, not LF
    ser.write(b"load_image_test\r")
    print(read_until(ser, b"> "), end="")  # banner text, up to the command's one prompt

    hex_str = data.hex()
    ser.write(hex_str.encode("ascii") + b"\r\n")
    # The command shows exactly one prompt (above) then either displays the
    # frame and waits for CTRL+C, or reports an error and exits - either way
    # there's no second prompt to wait for, just read whatever comes back.
    print(read_for(ser, 1.0), end="")


def main():
    parser = argparse.ArgumentParser(
        description="Send an image to the device's load_image_test CLI command for display")
    parser.add_argument("image", help="Path to the image to display (any format Pillow can read)")
    parser.add_argument("--port", default=PORT, help=f"Serial port (default: {PORT})")
    parser.add_argument("--baud", type=int, default=BAUD, help=f"Baud rate (default: {BAUD})")
    parser.add_argument("--width", type=int, default=DISPLAY_WIDTH, help="Display width in pixels")
    parser.add_argument("--height", type=int, default=DISPLAY_HEIGHT, help="Display height in pixels")
    parser.add_argument("--levels", type=int, default=GRAY_LEVELS, help="Number of gray levels to quantize to")
    args = parser.parse_args()

    data = to_display_buffer(args.image, args.width, args.height, args.levels)

    with serial.Serial(args.port, args.baud, timeout=0.1) as ser:
        ser.reset_input_buffer()
        send_image(ser, data)

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


if __name__ == "__main__":
    main()
