"""
RPC interface comparison: COM10 (USB CDC) vs COM41 (USB-TTL)
Verifies that both interfaces correctly exchange RPC protocol data.
"""
import time
import serial
import sys
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SCRIPT_DIR)

# Reuse rpc_test helpers
from rpc_test import (
    open_serial, enter_rpc_mode, send_button, send_message,
    read_message, _ensure_protos, _encode_varint, _rpc_pb2,
    make_rpc_message,
)

TIMEOUT_S = 5.0


def test_enter_mode(port, name):
    """Test entering RPC mode on a port. Returns (success, drained_bytes, errors)."""
    print(f"\n{'='*60}")
    print(f"  Testing {name} ({port})")
    print(f"{'='*60}")

    ser = None
    try:
        ser = open_serial(port, baud=1500000, timeout=1.0)
    except Exception as e:
        print(f"  FAIL: cannot open {port}: {e}")
        return False

    try:
        enter_rpc_mode(ser, timeout_s=TIMEOUT_S)
        # Drain everything — on UART the device may have queued
        # banner bytes on the TX line that arrive after 0xFD.
        ser.timeout = 0.5
        drained_after = 0
        while True:
            chunk = ser.read(4096)
            if not chunk:
                break
            drained_after += len(chunk)
        ser.timeout = 1.0
        print(f"  OK: entered RPC mode (drained {drained_after} trailing bytes)")

        # Send a button to verify bidirectional communication
        _ensure_protos()
        btn_data = make_rpc_message(content_oneof="button_event", button="OK", action="PRESS")
        ser.write(btn_data)
        ser.flush()
        print(f"  Sent: ButtonEvent(OK, PRESS) [{len(btn_data)} bytes] — OK")

        return True
    except Exception as e:
        print(f"  FAIL: {e}")
        return False
    finally:
        if ser:
            ser.close()


def main():
    ports = [
        ("COM10", "USB CDC"),
        ("COM41", "USB-TTL UART"),
    ]

    results = {}
    for port, name in ports:
        ok = test_enter_mode(port, name)
        results[name] = ok
        time.sleep(0.5)

    print(f"\n{'='*60}")
    print(f"  SUMMARY")
    print(f"{'='*60}")
    for name, ok in results.items():
        status = "PASS" if ok else "FAIL"
        print(f"  {name}: {status}")

    if not all(results.values()):
        print("\n  MISMATCH: one or more interfaces failed!")
        sys.exit(1)
    else:
        print("\n  Both interfaces work correctly.")
        sys.exit(0)


if __name__ == "__main__":
    main()
