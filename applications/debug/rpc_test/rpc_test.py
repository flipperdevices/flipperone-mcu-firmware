"""
RPC test script for Flipper One firmware.
Sends button/touch events and receives display frames over serial (COM10).

Usage:
    python rpc_test.py                         # interactive mode
    python rpc_test.py --button OK             # send a single button press
    python rpc_test.py --touch START 512 384 1000  # send a touch event
    python rpc_test.py --listen 5              # listen for 5 seconds

Requires: pyserial, protobuf, grpcio-tools
"""

import argparse
import atexit
import os
import struct
import sys
import time
import threading
import queue

# ── Serial port config ──────────────────────────────────────────────────────
PORT = "COM10"
BAUD = 1500000
TIMEOUT = 1.0

# ── Session state ───────────────────────────────────────────────────────────
_rpc_session_open = False
_ser_handle = None  # stored for atexit cleanup

# ── Proto compilation ───────────────────────────────────────────────────────
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROTO_DIR = os.path.normpath(os.path.join(SCRIPT_DIR, "..", "..", "..", "assets", "proto"))
PB2_DIR = os.path.join(SCRIPT_DIR, "pb2")

PROTO_FILES = ["rpc.proto", "input.proto", "frame.proto"]


def _compile_protos():
    """Compile .proto files to Python _pb2 modules using grpc_tools.protoc."""
    import grpc_tools.protoc

    os.makedirs(PB2_DIR, exist_ok=True)

    proto_args = [
        "grpc_tools.protoc",
        f"--proto_path={PROTO_DIR}",
        f"--python_out={PB2_DIR}",
    ] + [os.path.join(PROTO_DIR, p) for p in PROTO_FILES]

    result = grpc_tools.protoc.main(proto_args)
    if result != 0:
        raise RuntimeError(f"protoc failed with exit code {result}")

    # grpc_tools.protoc.main mutates sys.path; clean up
    if PB2_DIR not in sys.path:
        sys.path.insert(0, PB2_DIR)


def _import_pb2(name):
    """Import a compiled _pb2 module by its proto basename."""
    return __import__(f"{name}_pb2", fromlist=[name])


# Lazy-load pb2 modules
_rpc_pb2 = None
_input_pb2 = None
_frame_pb2 = None


def _ensure_protos():
    global _rpc_pb2, _input_pb2, _frame_pb2
    if _rpc_pb2 is not None:
        return

    # Check if compiled modules exist; compile if needed
    if not all(os.path.isfile(os.path.join(PB2_DIR, f"{n}_pb2.py")) for n in ["rpc", "input", "frame"]):
        print("Compiling .proto files...")
        _compile_protos()

    if PB2_DIR not in sys.path:
        sys.path.insert(0, PB2_DIR)

    _rpc_pb2 = _import_pb2("rpc")
    _input_pb2 = _import_pb2("input")
    _frame_pb2 = _import_pb2("frame")


# ── Varint helpers (protobuf wire format) ───────────────────────────────────
def _encode_varint(value):
    """Encode a 64-bit unsigned integer as protobuf varint."""
    buf = []
    while True:
        byte = value & 0x7F
        value >>= 7
        if value:
            byte |= 0x80
        buf.append(byte)
        if not value:
            break
    return bytes(buf)


def _decode_varint(data, offset=0):
    """Decode a protobuf varint from data starting at offset.
    Returns (value, bytes_consumed)."""
    value = 0
    shift = 0
    i = offset
    while i < len(data):
        byte = data[i]
        value |= (byte & 0x7F) << shift
        shift += 7
        i += 1
        if not (byte & 0x80):
            return value, i - offset
    raise ValueError("Truncated varint")


# ── Message builders ────────────────────────────────────────────────────────
def make_rpc_message(content_oneof=None, **kwargs):
    """Build and return a serialised RpcMessage (with delimited length prefix).

    content_oneof: 'frame', 'button_event' or 'touch_event'
    kwargs: passed to the nested message constructor.
    """
    _ensure_protos()

    msg = _rpc_pb2.RpcMessage()

    if content_oneof == "button_event":
        btn = getattr(_input_pb2, kwargs.pop("button", "OK"))
        action = getattr(_input_pb2, kwargs.pop("action", "PRESS"))
        evt = _input_pb2.ButtonEvent(button=btn, action=action)
        msg.button_event.CopyFrom(evt)
    elif content_oneof == "touch_event":
        ttype = getattr(_input_pb2, kwargs.pop("type", "START"))
        evt = _input_pb2.TouchEvent(
            type=ttype,
            x=kwargs.pop("x", 512),
            y=kwargs.pop("y", 384),
            pressure=kwargs.pop("pressure", 1000),
        )
        msg.touch_event.CopyFrom(evt)
    elif content_oneof == "frame":
        frame = _frame_pb2.Frame(
            width=kwargs.get("width", 258),
            height=kwargs.get("height", 144),
            encoding=getattr(_frame_pb2, kwargs.get("encoding", "PLAIN")),
            pixel_format=getattr(_frame_pb2, kwargs.get("pixel_format", "L8")),
            data=kwargs.get("data", b""),
        )
        msg.frame.CopyFrom(frame)
    else:
        raise ValueError(f"Unknown content type: {content_oneof}")

    payload = msg.SerializeToString()
    return _encode_varint(len(payload)) + payload


def parse_rpc_message(data):
    """Parse a raw byte buffer into an RpcMessage dict.
    Expects the buffer to start after the length delimiter."""
    _ensure_protos()
    msg = _rpc_pb2.RpcMessage()
    msg.ParseFromString(data)

    result = {"which": msg.WhichOneof("content")}

    if result["which"] == "button_event":
        evt = msg.button_event
        result["button"] = _input_pb2.Button.Name(evt.button)
        result["action"] = _input_pb2.ButtonAction.Name(evt.action)
    elif result["which"] == "touch_event":
        evt = msg.touch_event
        result["type"] = _input_pb2.TouchType.Name(evt.type)
        result["x"] = evt.x
        result["y"] = evt.y
        result["pressure"] = evt.pressure
    elif result["which"] == "frame":
        f = msg.frame
        result["width"] = f.width
        result["height"] = f.height
        result["encoding"] = _frame_pb2.Encoding.Name(f.encoding)
        result["pixel_format"] = _frame_pb2.PixelFormat.Name(f.pixel_format)
        result["data_len"] = len(f.data)
        result["data"] = f.data

    return result


# ── Serial I/O ──────────────────────────────────────────────────────────────
import serial


def _cleanup():
    """atexit handler: send RPC session close if serial port is still open."""
    global _rpc_session_open, _ser_handle
    if _ser_handle is not None and _rpc_session_open:
        try:
            send_rpc_session_close(_ser_handle)
        except BaseException:
            pass
    if _ser_handle is not None:
        try:
            _ser_handle.close()
        except BaseException:
            pass
        _ser_handle = None


# Register cleanup handlers
atexit.register(_cleanup)


def open_serial(port=PORT, baud=BAUD, timeout=TIMEOUT):
    """Open serial port."""
    global _ser_handle
    ser = serial.Serial(port, baud, timeout=timeout)
    ser.reset_input_buffer()
    ser.reset_output_buffer()
    # Send CRLF to wake up / ensure CLI prompt is ready
    ser.write(b"\r\n")
    ser.flush()
    time.sleep(0.05)
    ser.reset_input_buffer()
    _ser_handle = ser
    print(f"Connected to {port} at {baud} baud")
    return ser


def enter_rpc_mode(ser, cmd="rpc", timeout_s=5.0):
    """Send CLI command to enter RPC mode and wait for ready marker (0xFD).
    Returns True if RPC session was successfully opened."""
    global _rpc_session_open
    full_cmd = (cmd + "\r").encode()
    ser.write(full_cmd)
    ser.flush()

    # Wait for the 0xFD ready marker (discard everything before it)
    ser.timeout = 0.5
    deadline = time.monotonic() + timeout_s
    drained = 0
    while time.monotonic() < deadline:
        byte = ser.read(1)
        if not byte:
            continue
        if byte[0] == 0xFD:
            ser.timeout = TIMEOUT
            _rpc_session_open = True
            print(f"Entered RPC mode (sent '{cmd}') [drained {drained} bytes]")
            return True
        drained += 1

    ser.timeout = TIMEOUT
    print(f"WARNING: RPC ready marker not received within {timeout_s}s")
    return False


def send_message(ser, raw_bytes):
    """Write raw bytes to serial."""
    ser.write(raw_bytes)
    ser.flush()


def read_message(ser):
    """Read one delimited protobuf message from serial.
    Returns parsed message dict, or None if no message available."""
    # Read varint length byte-by-byte
    length = 0
    shift = 0
    for _ in range(10):  # max 10 bytes for varint64
        byte = ser.read(1)
        if not byte:
            return None
        b = byte[0]
        length |= (b & 0x7F) << shift
        shift += 7
        if not (b & 0x80):
            break
    else:
        raise ValueError("Varint too long")

    if length == 0:
        return None

    payload = b""
    while len(payload) < length:
        chunk = ser.read(length - len(payload))
        if not chunk:
            return None  # timeout or disconnected
        payload += chunk

    try:
        return parse_rpc_message(payload)
    except Exception:
        return None


# ── Test actions ────────────────────────────────────────────────────────────
def send_button(ser, button="OK", action="PRESS"):
    """Send a button event."""
    data = make_rpc_message(
        content_oneof="button_event", button=button, action=action
    )
    send_message(ser, data)
    print(f"Sent: ButtonEvent({button}, {action})  [{len(data)} bytes]")


def send_touch(ser, ttype="START", x=512, y=384, pressure=1000):
    """Send a touch event."""
    data = make_rpc_message(
        content_oneof="touch_event", type=ttype, x=x, y=y, pressure=pressure
    )
    send_message(ser, data)
    print(f"Sent: TouchEvent({ttype}, x={x}, y={y}, p={pressure})  [{len(data)} bytes]")


def send_start_virtual_display(ser):
    """Send start_virtual_display_request."""
    _ensure_protos()
    msg = _rpc_pb2.RpcMessage()
    msg.start_virtual_display_request.SetInParent()
    payload = msg.SerializeToString()
    data = _encode_varint(len(payload)) + payload
    send_message(ser, data)
    print(f"Sent: StartVirtualDisplayRequest  [{len(data)} bytes]")


def send_stop_virtual_display(ser):
    """Send stop_virtual_display_request."""
    _ensure_protos()
    msg = _rpc_pb2.RpcMessage()
    msg.stop_virtual_display_request.SetInParent()
    payload = msg.SerializeToString()
    data = _encode_varint(len(payload)) + payload
    send_message(ser, data)
    print(f"Sent: StopVirtualDisplayRequest  [{len(data)} bytes]")


def send_rpc_session_close(ser):
    """Send rpc_session_close_request."""
    global _rpc_session_open
    _ensure_protos()
    msg = _rpc_pb2.RpcMessage()
    msg.rpc_session_close_request.SetInParent()
    payload = msg.SerializeToString()
    data = _encode_varint(len(payload)) + payload
    send_message(ser, data)
    _rpc_session_open = False
    print(f"Sent: RpcSessionCloseRequest  [{len(data)} bytes]")


def listen_frames(ser, duration_s=5.0, display=False):
    """Listen for incoming frames for `duration` seconds.
    If display=True, shows frames in a movable tkinter window.
    Serial reading runs on a background thread; tkinter on the main thread."""
    viewer = _FrameViewer() if display else None
    stop_event = threading.Event()

    def _read_loop():
        start = time.monotonic()
        count = 0
        end_time = start + duration_s
        while not stop_event.is_set() and time.monotonic() < end_time:
            try:
                msg = read_message(ser)
            except Exception as e:
                print(f"Skipping bad data: {e}")
                ser.reset_input_buffer()
                msg = None
            if msg is None:
                continue
            count += 1
            if msg["which"] == "frame":
                info = (
                    f"[{count}] Frame: {msg['width']}x{msg['height']}, "
                    f"enc={msg['encoding']}, fmt={msg['pixel_format']}, "
                    f"data={msg['data_len']} bytes"
                )
                print(info)
                if viewer and msg["data_len"] > 0:
                    viewer.show(msg["data"], msg["width"], msg["height"])
                    if not viewer.is_open():
                        print("Window closed, stopping.")
                        stop_event.set()
                        break
            else:
                print(f"[{count}] {msg}")
        print(f"Received {count} messages in {duration_s}s")

    thread = threading.Thread(target=_read_loop, daemon=True)
    thread.start()

    if viewer:
        # Auto-close after duration
        viewer.root.after(int(duration_s * 1000), viewer.close)
        try:
            viewer.root.mainloop()
        except KeyboardInterrupt:
            pass
        stop_event.set()
    else:
        thread.join(timeout=duration_s + 2.0)
        stop_event.set()

    thread.join(timeout=2.0)
    if viewer:
        viewer.close()


class _FrameViewer:
    """Display L8 grayscale frames (tkinter runs on main thread, queue-based)."""

    def __init__(self):
        import tkinter as tk
        from PIL import Image, ImageTk

        self.tk = tk
        self.Image = Image
        self.ImageTk = ImageTk

        self._queue = queue.Queue(maxsize=2)
        self._stop_event = threading.Event()

        self.root = tk.Tk()
        self.root.title("Flipper One - Screen Stream")
        self.label = tk.Label(self.root)
        self.label.pack()
        self.root.geometry("+%d+50" % (self.root.winfo_screenwidth() - 400))
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)
        self._poll_queue()

    def _on_close(self):
        self._stop_event.set()
        self.root.destroy()

    def _poll_queue(self):
        try:
            while True:
                data, width, height = self._queue.get_nowait()
                self._display(data, width, height)
        except queue.Empty:
            pass
        if not self._stop_event.is_set():
            self.root.after(30, self._poll_queue)

    def _display(self, data, width, height):
        # Build grayscale image from L8 framebuffer data
        img = self.Image.new("L", (width, height))
        img.putdata(data)
        # Colorize: black → dark brown, white → bright orange
        from PIL import ImageOps
        img = ImageOps.colorize(img, black="#1a0800", white="#FF8C00")
        # Scale up for comfortable viewing
        scale = max(1, min(4, 800 // width))
        if scale > 1:
            img = img.resize((width * scale, height * scale), self.Image.NEAREST)
        photo = self.ImageTk.PhotoImage(img)
        self.label.config(image=photo)
        self.label.image = photo
        self.root.geometry(f"{width * scale + 20}x{height * scale + 20}")

    def show(self, data, width, height):
        """Enqueue a new frame for display (non-blocking, thread-safe)."""
        try:
            self._queue.put_nowait((data, width, height))
        except queue.Full:
            pass

    def is_open(self):
        try:
            return self.root.winfo_exists()
        except (self.tk.TclError, RuntimeError):
            return False

    def close(self):
        self._stop_event.set()
        try:
            self.root.destroy()
        except (self.tk.TclError, RuntimeError):
            pass


def _display_frame_ascii(data, width, height):
    """Render an L8 frame as ASCII grayscale art (fallback)."""
    chars = " .:-=+*#%@"
    step = max(1, len(data) // (80 * 20))
    for y in range(0, min(height, 200), step * 2):
        line = ""
        for x in range(0, min(width, 80), step):
            idx = y * width + x
            if idx < len(data):
                p = data[idx] / 255.0
                line += chars[int(p * (len(chars) - 1))]
        print(line)


# ── CLI ─────────────────────────────────────────────────────────────────────
def interactive_mode(ser):
    """Simple interactive test loop."""
    print("\nCommands: button <NAME> [PRESS|RELEASE], touch <START|MOVE|END> <x> <y> <p>, listen <N>, quit")
    print("  start_vd, stop_vd, close, stream [N]")
    print("Example: button OK PRESS")
    while True:
        try:
            line = input("> ").strip()
        except (EOFError, KeyboardInterrupt):
            break
        if not line:
            continue
        parts = line.split()
        cmd = parts[0].lower()

        if cmd == "quit" or cmd == "exit":
            break
        elif cmd == "button":
            btn = parts[1].upper() if len(parts) > 1 else "OK"
            act = parts[2].upper() if len(parts) > 2 else "PRESS"
            send_button(ser, btn, act)
        elif cmd == "touch":
            ttype = parts[1].upper() if len(parts) > 1 else "START"
            x = int(parts[2]) if len(parts) > 2 else 512
            y = int(parts[3]) if len(parts) > 3 else 384
            p = int(parts[4]) if len(parts) > 4 else 1000
            send_touch(ser, ttype, x, y, p)
        elif cmd == "listen":
            duration = float(parts[1]) if len(parts) > 1 else 5.0
            listen_frames(ser, duration)
        elif cmd == "start_vd":
            send_start_virtual_display(ser)
            ser.reset_input_buffer()  # flush leftover bytes from previous session
            print("Streaming to window — close window to stop")
            try:
                listen_frames(ser, 30.0, display=True)
            except KeyboardInterrupt:
                pass
        elif cmd == "stop_vd":
            send_stop_virtual_display(ser)
        elif cmd == "open":
            global _rpc_session_open
            _rpc_session_open = False
            ser.close()
            time.sleep(0.3)
            ser.open()
            ser.write(b"\r\n")
            ser.flush()
            time.sleep(0.2)
            ser.reset_input_buffer()
            enter_rpc_mode(ser)
            ser.reset_input_buffer()
        elif cmd == "close":
            send_rpc_session_close(ser)
        elif cmd == "stream":
            duration = float(parts[1]) if len(parts) > 1 else 5.0
            ser.reset_input_buffer()
            send_start_virtual_display(ser)
            listen_frames(ser, duration, display=True)
            send_stop_virtual_display(ser)
        elif cmd == "help":
            print("Commands:")
            print("  button <OK|BACK|KEY_1|KEY_2|POWER|KEY_4|KEY_5|SW|DOWN|RIGHT|LEFT|UP|PTT> [PRESS|RELEASE]")
            print("  touch <START|MOVE|END> <x> <y> <pressure>")
            print("  listen <seconds>")
            print("  start_vd  — start virtual display streaming")
            print("  stop_vd   — stop virtual display streaming")
            print("  stream [N] — start_vd + listen N seconds")
            print("  open      — enter RPC mode (after close)")
            print("  close     — close RPC session")
            print("  quit")
        else:
            print(f"Unknown: {cmd}")


def main():
    parser = argparse.ArgumentParser(description="Flipper One RPC test over serial")
    parser.add_argument("--port", default=PORT, help=f"Serial port (default: {PORT})")
    parser.add_argument("--baud", type=int, default=BAUD, help=f"Baud rate (default: {BAUD})")
    parser.add_argument("--button", nargs=1,
                        help="Send button event: OK, BACK, KEY_1, KEY_2, POWER, KEY_4, KEY_5, SW, DOWN, RIGHT, LEFT, UP, PTT")
    parser.add_argument("--action", default="PRESS", help="Button action (PRESS/RELEASE)")
    parser.add_argument("--touch", nargs=4, metavar=("TYPE", "X", "Y", "P"),
                        help="Send touch event: TYPE( START|MOVE|END) X Y PRESSURE")
    parser.add_argument("--listen", type=float, default=0, help="Listen for N seconds after sending")
    parser.add_argument("--stream", action="store_true",
                        help="Start virtual display streaming and display frames")
    parser.add_argument("--display", action="store_true", default=True,
                        help="Render received frames as ASCII art (default: on)")
    parser.add_argument("--no-display", action="store_false", dest="display",
                        help="Skip ASCII rendering")
    parser.add_argument("--auto-rpc", action="store_true", default=True,
                        help="Auto-enter RPC mode by sending 'rpc' command (default: on)")
    parser.add_argument("--no-auto-rpc", action="store_false", dest="auto_rpc",
                        help="Skip automatic RPC mode entry")
    args = parser.parse_args()

    _ensure_protos()
    ser = open_serial(args.port, args.baud)

    try:
        if args.auto_rpc:
            enter_rpc_mode(ser)
        # Clear any residual data that arrived after the 0xFD marker
        ser.reset_input_buffer()

        if args.button:
            send_button(ser, args.button[0].upper(), args.action.upper())
            if args.listen:
                listen_frames(ser, args.listen, args.display)
        elif args.touch:
            send_touch(ser, args.touch[0].upper(), int(args.touch[1]), int(args.touch[2]), int(args.touch[3]))
            if args.listen:
                listen_frames(ser, args.listen, args.display)
        elif args.stream:
            ser.reset_input_buffer()
            send_start_virtual_display(ser)
            time.sleep(0.1)  # let device start the stream thread
            try:
                listen_frames(ser, args.listen or 10.0, args.display)
            except KeyboardInterrupt:
                pass
            send_stop_virtual_display(ser)
            send_rpc_session_close(ser)
        else:
            interactive_mode(ser)
    except KeyboardInterrupt:
        print("\nInterrupted.")
    finally:
        # Always try to close — even if _rpc_session_open is False,
        # the device may have a leftover session from a previous unclean exit.
        try:
            if _rpc_session_open:
                send_rpc_session_close(ser)
        except BaseException:
            pass
        _rpc_session_open = False
        try:
            ser.close()
        except BaseException:
            pass
        print("Disconnected.")


if __name__ == "__main__":
    main()
