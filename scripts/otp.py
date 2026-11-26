#!/usr/bin/env python3
"""Build, sign and check the Flipper One MCU OTP blocks (see docs/OTP.md).

A block is printed with its raw hex, or saved to a file with -o, and written
on the device with `otp program OTP<n> <hex>`:

    otp.py create-otp1 --hw-version 1 --target 2 --body 1 --connect 1 --chip-id <rp2350_hardware_uid> -o otp1.bin
    otp.py create-otp2 --eth0 ... --name ... -o otp2.bin
    otp.py create-otp3 --key key.pem --key-id 0 --otp1 otp1.bin --otp2 otp2.bin -o otp3.bin
    otp.py verify --otp1 otp1.bin --otp2 otp2.bin --otp3 otp3.bin
    otp.py load otp1.bin

Keep the files: a write cut short is finished by sending the same block
again, byte for byte. A block made anew differs (its timestamp at least) and
the device refuses it, so -o never overwrites a file.

Keys and signatures need the `cryptography` package: pip install -r scripts/requirements-otp.txt
"""

import argparse
import os
import re
import struct
import sys
import time
from dataclasses import dataclass, fields
from typing import ClassVar

MAGIC = 0x544F3146  # "F1OT"
HEADER = "<IHHQ"  # magic, index, version, timestamp
MAC_NAMES = ["eth0", "eth1", "usb", "wifi_2g4_sta", "wifi_5g_sta", "wifi_2g4_ap", "wifi_5g_ap", "bt"]
CURVE_SECP256R1 = 1
KEYS_HEADER = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "targets", "f1", "furi_hal", "furi_hal_otp_keys.h")


def crc16(data: bytes) -> int:
    """CRC-16/CCITT-FALSE"""
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def c_string(raw: bytes) -> str:
    return raw.split(b"\0", 1)[0].decode("ascii")


@dataclass
class Block:
    """A block as furi_hal_otp.h lays it out: header, fields, CRC"""

    INDEX: ClassVar[int]
    VERSION: ClassVar[int] = 1
    FORMAT: ClassVar[str]  # fields after the header, before the CRC
    SIZE: ClassVar[int]

    timestamp: int

    def values(self) -> tuple:
        raise NotImplementedError

    @classmethod
    def from_values(cls, timestamp: int, values: tuple) -> "Block":
        raise NotImplementedError

    def to_bytes(self) -> bytes:
        body = struct.pack(HEADER, MAGIC, self.INDEX, self.VERSION, self.timestamp) + struct.pack(self.FORMAT, *self.values())
        # The CRC covers everything but the magic and itself
        data = body + struct.pack("<H", crc16(body[4:]))
        assert len(data) == self.SIZE, (self.INDEX, len(data))
        return data

    @classmethod
    def from_bytes(cls, data: bytes) -> "Block":
        if len(data) < struct.calcsize(HEADER):
            raise ValueError("too short for an OTP block")
        magic, index, version, timestamp = struct.unpack_from(HEADER, data)
        if magic != MAGIC:
            raise ValueError(f"magic {magic:#010x}, expected {MAGIC:#010x}")
        if cls is Block:
            if index not in BLOCKS:
                raise ValueError(f"index {index}")
            return BLOCKS[index].from_bytes(data)
        if index != cls.INDEX:
            raise ValueError(f"index {index}, expected {cls.INDEX}")
        if len(data) != cls.SIZE:
            raise ValueError(f"OTP{index} is {cls.SIZE} bytes, got {len(data)}")
        if version != cls.VERSION:
            raise ValueError(f"OTP{index} version {version}, the firmware knows {cls.VERSION}")
        (crc,) = struct.unpack_from("<H", data, len(data) - 2)
        if crc != crc16(data[4:-2]):
            raise ValueError(f"OTP{index}: CRC mismatch")
        return cls.from_values(timestamp, struct.unpack_from(cls.FORMAT, data, struct.calcsize(HEADER)))

    def describe(self):
        """(name, text) of each field, for printing"""
        for field in fields(self):
            value = getattr(self, field.name)
            yield field.name, value.hex() if isinstance(value, bytes) else str(value)


@dataclass
class Otp1(Block):
    """The board"""

    INDEX = 1
    FORMAT = "<HHHH8s"
    SIZE = 34

    hw_version: int
    target: int
    body: int
    connect: int
    chip_id: bytes

    def values(self):
        return self.hw_version, self.target, self.body, self.connect, self.chip_id

    @classmethod
    def from_values(cls, timestamp, values):
        return cls(timestamp, *values)


@dataclass
class Otp2(Block):
    """The device"""

    INDEX = 2
    FORMAT = "<48s32s32sH2s16sIIH"
    SIZE = 160

    macs: list  # 6 bytes each, in MAC_NAMES order
    name: str
    model: str
    color: int
    region: str
    cpu_id: bytes
    ram_gb: int
    ufs_gb: int
    display: int

    def values(self):
        return (
            b"".join(self.macs),
            self.name.encode("ascii"),
            self.model.encode("ascii"),
            self.color,
            self.region.encode("ascii"),
            self.cpu_id,
            self.ram_gb,
            self.ufs_gb,
            self.display,
        )

    @classmethod
    def from_values(cls, timestamp, values):
        macs, name, model, color, region, cpu_id, ram_gb, ufs_gb, display = values
        macs = [macs[i * 6 : i * 6 + 6] for i in range(len(MAC_NAMES))]
        return cls(timestamp, macs, c_string(name), c_string(model), color, region.decode("ascii"), cpu_id, ram_gb, ufs_gb, display)

    def describe(self):
        for name, text in super().describe():
            if name == "macs":
                for mac_name, mac in zip(MAC_NAMES, self.macs):
                    yield mac_name, mac.hex(":")
            else:
                yield name, text


@dataclass
class Otp3(Block):
    """The signatures of OTP1 and OTP2, with the key that made them"""

    INDEX = 3
    FORMAT = "<HH64s64s64s"
    SIZE = 214

    key_id: int
    curve: int
    public_key: bytes  # X || Y
    otp1_signature: bytes  # r || s
    otp2_signature: bytes

    def values(self):
        return self.key_id, self.curve, self.public_key, self.otp1_signature, self.otp2_signature

    @classmethod
    def from_values(cls, timestamp, values):
        return cls(timestamp, *values)


BLOCKS = {cls.INDEX: cls for cls in (Otp1, Otp2, Otp3)}


def firmware_keys() -> list:
    """Public keys the firmware carries, by key id, from furi_hal_otp_keys.h"""
    with open(KEYS_HEADER) as f:
        text = re.sub(r"//[^\n]*", "", f.read())
    table = text[text.index("furi_hal_otp_keys") :]
    table = table[table.index("{") + 1 : table.index("};")]
    return [bytes(int(b, 16) for b in re.findall(r"0x([0-9a-fA-F]{2})", entry)) for entry in re.findall(r"\{([^{}]*)\}", table)]


def show(block: Block, output: str = None):
    """Print a block's fields; save it to output, or print its raw hex"""
    data = block.to_bytes()
    print(f"OTP{block.INDEX} data:")
    for name, text in block.describe():
        print(f"  {name:14} {text}")
    if output:
        try:
            with open(output, "xb") as f:
                f.write(data)
        except FileExistsError:
            raise SystemExit(f"{output} exists: send that one, or remove it to make a new block")
        print(f"OTP{block.INDEX} saved to: {output}")
    else:
        print(f"  Raw: {data.hex()}")


def parse_mac(text: str) -> bytes:
    parts = text.split(":")
    mac = bytes(int(p, 16) for p in parts)
    if len(parts) != 6 or len(mac) != 6 or not any(mac) or mac[0] & 1:
        raise argparse.ArgumentTypeError(f"not an interface MAC address: {text}")
    return mac


def parse_str(text: str) -> str:
    if len(text.encode("ascii")) > 31:
        raise argparse.ArgumentTypeError(f"longer than 31 chars: {text}")
    return text


def parse_region(text: str) -> str:
    if len(text.encode("ascii")) != 2:
        raise argparse.ArgumentTypeError("two letters")
    return text


def parse_hex(size: int):
    def parse(text: str) -> bytes:
        raw = bytes.fromhex(text)
        if len(raw) != size:
            raise argparse.ArgumentTypeError(f"{size} bytes of hex expected")
        return raw

    return parse


def signed_data(chip_id: bytes, block: Block) -> bytes:
    return chip_id + f"-otp{block.INDEX}-".encode() + block.to_bytes()


def read_block(path: str, cls=Block) -> Block:
    with open(path, "rb") as f:
        return cls.from_bytes(f.read())


def public_bytes(private_key) -> bytes:
    numbers = private_key.public_key().public_numbers()
    return numbers.x.to_bytes(32, "big") + numbers.y.to_bytes(32, "big")


def write_new(path: str, data: bytes, mode: int):
    """Create a file that does not exist yet, with these permissions whatever the umask"""
    try:
        fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, mode)
    except FileExistsError:
        raise SystemExit(f"{path} exists: remove it to make a new one")
    with os.fdopen(fd, "wb") as f:
        f.write(data)


def cmd_generate_key(args):
    from cryptography.hazmat.primitives import serialization
    from cryptography.hazmat.primitives.asymmetric import ec

    key = ec.generate_private_key(ec.SECP256R1())
    pem = key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.NoEncryption())
    write_new(args.output, pem, 0o600)
    print(f"Private key saved to: {args.output}")
    if args.public:
        write_new(args.public, public_bytes(key), 0o644)
        print(f"Public key (raw) saved to: {args.public}")
    print(f"Public key hex: {public_bytes(key).hex()}")


def cmd_create_otp1(args):
    show(Otp1(args.timestamp, args.hw_version, args.target, args.body, args.connect, args.chip_id), args.output)


def cmd_create_otp2(args):
    macs = [getattr(args, name) for name in MAC_NAMES]
    block = Otp2(args.timestamp, macs, args.name, args.model, args.color, args.region, args.cpu_id, args.ram_gb, args.ufs_gb, args.display)
    show(block, args.output)


def cmd_create_otp3(args):
    from cryptography.hazmat.primitives import hashes, serialization
    from cryptography.hazmat.primitives.asymmetric import ec
    from cryptography.hazmat.primitives.asymmetric.utils import decode_dss_signature

    with open(args.key, "rb") as f:
        key = serialization.load_pem_private_key(f.read(), password=None)
    keys = firmware_keys()
    if args.key_id >= len(keys) or keys[args.key_id] != public_bytes(key):
        raise SystemExit(f"--key is not key id {args.key_id} of {KEYS_HEADER}: the device would refuse the block")
    otp1, otp2 = read_block(args.otp1, Otp1), read_block(args.otp2, Otp2)
    signatures = []
    for block in (otp1, otp2):
        # Deterministic (RFC 6979): the same blocks and key give the same OTP3
        ecdsa = ec.ECDSA(hashes.SHA256(), deterministic_signing=True)
        r, s = decode_dss_signature(key.sign(signed_data(otp1.chip_id, block), ecdsa))
        signatures.append(r.to_bytes(32, "big") + s.to_bytes(32, "big"))
    show(Otp3(args.timestamp, args.key_id, CURVE_SECP256R1, public_bytes(key), *signatures), args.output)


def cmd_verify(args):
    from cryptography.exceptions import InvalidSignature
    from cryptography.hazmat.primitives import hashes
    from cryptography.hazmat.primitives.asymmetric import ec
    from cryptography.hazmat.primitives.asymmetric.utils import encode_dss_signature

    otp1, otp2, otp3 = read_block(args.otp1, Otp1), read_block(args.otp2, Otp2), read_block(args.otp3, Otp3)
    if otp3.curve != CURVE_SECP256R1:
        raise SystemExit(f"OTP3 curve {otp3.curve}: only {CURVE_SECP256R1} (P-256) is known")
    public = ec.EllipticCurvePublicKey.from_encoded_point(ec.SECP256R1(), b"\x04" + otp3.public_key)
    ok = True
    for block, signature in ((otp1, otp3.otp1_signature), (otp2, otp3.otp2_signature)):
        der = encode_dss_signature(int.from_bytes(signature[:32], "big"), int.from_bytes(signature[32:], "big"))
        try:
            public.verify(der, signed_data(otp1.chip_id, block), ec.ECDSA(hashes.SHA256()))
            print(f"OTP{block.INDEX} signature: valid")
        except InvalidSignature:
            print(f"OTP{block.INDEX} signature: INVALID")
            ok = False
    keys = firmware_keys()
    if otp3.key_id < len(keys) and keys[otp3.key_id] == otp3.public_key:
        print(f"Key: the firmware's key id {otp3.key_id}")
    else:
        print(f"Key: NOT the firmware's key id {otp3.key_id}, the device refuses it")
        ok = False
    sys.exit(0 if ok else 1)


def cmd_load(args):
    show(read_block(args.file))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)
    now = int(time.time())

    s = sub.add_parser("generate-key", help="a P-256 key pair: private PEM to a new file, public key printed")
    s.add_argument("-o", "--output", required=True, help="private key file (PEM), created readable by its owner only")
    s.add_argument("--public", help="also save the public key, raw X || Y")
    s.set_defaults(func=cmd_generate_key)

    s = sub.add_parser("create-otp1", help="OTP1, the board")
    s.add_argument("-o", "--output", help="output file (if not given, prints hex data)")
    s.add_argument("--timestamp", type=int, default=now, help="(default: now)")
    s.add_argument("--hw-version", type=int, required=True)
    s.add_argument("--target", type=int, required=True, help="FW_TARGET number: f2 is 2")
    s.add_argument("--body", type=int, required=True)
    s.add_argument("--connect", type=int, required=True)
    s.add_argument("--chip-id", type=parse_hex(8), required=True, help="rp2350_hardware_uid, as `device_info` prints it")
    s.set_defaults(func=cmd_create_otp1)

    s = sub.add_parser("create-otp2", help="OTP2, the device")
    s.add_argument("-o", "--output", help="output file (if not given, prints hex data)")
    s.add_argument("--timestamp", type=int, default=now, help="(default: now)")
    for name in MAC_NAMES:
        s.add_argument(f"--{name.replace('_', '-')}", dest=name, type=parse_mac, required=True)
    s.add_argument("--name", type=parse_str, required=True)
    s.add_argument("--model", type=parse_str, required=True)
    s.add_argument("--color", type=int, required=True)
    s.add_argument("--region", type=parse_region, required=True, help="two letters")
    s.add_argument("--cpu-id", type=parse_hex(16), required=True)
    s.add_argument("--ram-gb", type=int, required=True)
    s.add_argument("--ufs-gb", type=int, required=True)
    s.add_argument("--display", type=int, required=True)
    s.set_defaults(func=cmd_create_otp2)

    s = sub.add_parser("create-otp3", help="OTP3, the signatures of OTP1 and OTP2")
    s.add_argument("-o", "--output", help="output file (if not given, prints hex data)")
    s.add_argument("--timestamp", type=int, default=now, help="(default: now)")
    s.add_argument("--key", required=True, help="private key file (PEM)")
    s.add_argument("--key-id", type=int, required=True, help="the key's number in furi_hal_otp_keys.h")
    s.add_argument("--otp1", required=True, help="OTP1 data file")
    s.add_argument("--otp2", required=True, help="OTP2 data file")
    s.set_defaults(func=cmd_create_otp3)

    s = sub.add_parser("verify", help="check OTP3's signatures, and that the firmware carries its key")
    s.add_argument("--otp1", required=True, help="OTP1 data file")
    s.add_argument("--otp2", required=True, help="OTP2 data file")
    s.add_argument("--otp3", required=True, help="OTP3 data file")
    s.set_defaults(func=cmd_verify)

    s = sub.add_parser("load", help="print an OTP data file")
    s.add_argument("file")
    s.set_defaults(func=cmd_load)

    args = p.parse_args()
    try:
        args.func(args)
    except ValueError as error:
        raise SystemExit(f"error: {error}")


if __name__ == "__main__":
    main()
