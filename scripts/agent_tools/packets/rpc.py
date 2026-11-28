"""RpcMessage wire format (assets/proto/rpc.proto), encoded by hand.

After the `rpc` command the MCU sends 0xFD, then both sides exchange
length-delimited messages; they are simple enough for no protobuf runtime.
"""

RPC_READY = 0xFD

# RpcMessage oneof field numbers
FIELD_FRAME = 1
FIELD_BUTTON_EVENT = 2
FIELD_TOUCH_EVENT = 3
FIELD_START_VIRTUAL_DISPLAY = 4
FIELD_STOP_VIRTUAL_DISPLAY = 5
FIELD_SESSION_CLOSE = 6

BUTTONS = {
    "ok": 0, "back": 1, "key1": 2, "key2": 3, "power": 4, "key4": 5, "key5": 6,
    "sw": 7, "down": 8, "right": 9, "left": 10, "up": 11, "ptt": 12,
}  # fmt: skip
ACTIONS = {"press": 0, "release": 1, "short": 2, "long": 3, "repeat": 4}
TOUCH_TYPES = {"start": 0, "move": 1, "end": 2}
# Touchpad coordinate range, from applications/services/input_touch/input_touch.h
TOUCH_MAX_X, TOUCH_MAX_Y, TOUCH_MAX_PRESSURE = 1023, 767, 1024 * 16 - 1


# Frame.encoding values (assets/proto/frame.proto)
ENCODING_PLAIN = 0
ENCODING_RUN_LENGTH = 1


def rle_decode(data):
    """Undo the MCU's RUN_LENGTH frame encoding, one byte per pixel.

    Format of lib/toolbox/rle_encode.c: an opcode byte with the high bit set is
    followed by that many (low 7 bits) bytes copied as they are; otherwise one
    byte follows, repeated that many times.
    """
    out, pos = bytearray(), 0
    while pos < len(data):
        opcode = data[pos]
        count = opcode & 0x7F
        pos += 1
        if opcode & 0x80:
            chunk = data[pos : pos + count]
            if len(chunk) != count:
                raise ValueError("RUN_LENGTH frame ends inside a verbatim run")
            out += chunk
            pos += count
        else:
            if pos >= len(data):
                raise ValueError("RUN_LENGTH frame ends before a repeated byte")
            out += bytes([data[pos]]) * count
            pos += 1
    return bytes(out)


def varint(value):
    out = bytearray()
    while True:
        byte = value & 0x7F
        value >>= 7
        out.append(byte | (0x80 if value else 0))
        if not value:
            return bytes(out)


def field_varint(number, value):
    # proto3 omits zero values; the decoder fills them in
    return varint(number << 3) + varint(value) if value else b""


def field_bytes(number, payload):
    return varint((number << 3) | 2) + varint(len(payload)) + payload


def parse_message(payload):
    """Decode one message level into {field number: int or bytes}."""
    fields, pos = {}, 0

    def next_varint():
        nonlocal pos
        value = shift = 0
        while True:
            byte = payload[pos]
            pos += 1
            value |= (byte & 0x7F) << shift
            shift += 7
            if not byte & 0x80:
                return value

    while pos < len(payload):
        key = next_varint()
        number, wire_type = key >> 3, key & 7
        if wire_type == 0:
            fields[number] = next_varint()
        elif wire_type == 2:
            size = next_varint()
            fields[number] = payload[pos : pos + size]
            pos += size
        else:
            raise ValueError(f"Unexpected protobuf wire type {wire_type}")
    return fields
