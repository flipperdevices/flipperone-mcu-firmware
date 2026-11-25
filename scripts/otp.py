#!/usr/bin/env python3
"""Build, sign and check the Flipper One MCU OTP blocks.

The layout is targets/f1/furi_hal/furi_hal_otp.h. Blocks are built here,
saved to files, and written on the device with `otp program <n> <hex>`:

    otp.py create-1 --target 2 --body 1 --connect 1 --chip-id <otp chipid> -o b1.bin
    otp.py create-2 --eth0 ... --name ... -o b2.bin
    otp.py sign --key key.pem --key-id 0 --otp1 b1.bin --otp2 b2.bin -o b3.bin
    otp.py hex b1.bin        # the argument for `otp program 1`

Signing needs the `cryptography` package.
"""

import argparse
import hashlib
import struct
import sys
import time

MAGIC = 0x544F3146  # "F1OT"
HEADER = "<IHHQ"  # magic, index, version, timestamp
VERSIONS = {1: 1, 2: 1, 3: 1}
MAC_NAMES = ["eth0", "eth1", "usb", "wifi_2g4_sta", "wifi_5g_sta", "wifi_2g4_ap", "wifi_5g_ap", "bt"]

# Fields after the header, before the CRC (struct format, names)
BLOCK1 = ("<HHHH8s", ["hw_version", "target", "body", "connect", "chip_id"])
BLOCK2 = ("<48s32s32sH2s16sIIH", ["macs", "name", "model", "color", "region", "cpu_id", "ram_gb", "ufs_gb", "display"])
BLOCK3 = ("<H64s64s", ["key_id", "block1_signature", "block2_signature"])
LAYOUTS = {1: BLOCK1, 2: BLOCK2, 3: BLOCK3}
SIZES = {1: 34, 2: 160, 3: 148}


def crc16(data: bytes) -> int:
    """CRC-16/CCITT-FALSE"""
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def pack(index: int, timestamp: int, *fields) -> bytes:
    body = struct.pack(HEADER, MAGIC, index, VERSIONS[index], timestamp) + struct.pack(LAYOUTS[index][0], *fields)
    # The CRC covers everything but the magic and itself
    data = body + struct.pack("<H", crc16(body[4:]))
    assert len(data) == SIZES[index], (index, len(data))
    return data


def unpack(data: bytes, index: int) -> dict:
    if len(data) != SIZES[index]:
        raise ValueError(f"block {index} is {SIZES[index]} bytes, got {len(data)}")
    magic, idx, version, timestamp = struct.unpack_from(HEADER, data)
    if magic != MAGIC or idx != index or version != VERSIONS[index]:
        raise ValueError(f"block {index}: magic {magic:#x}, index {idx}, version {version}")
    (crc,) = struct.unpack_from("<H", data, len(data) - 2)
    if crc != crc16(data[4:-2]):
        raise ValueError(f"block {index}: CRC mismatch")
    fmt, names = LAYOUTS[index]
    values = struct.unpack_from(fmt, data, struct.calcsize(HEADER))
    return {"timestamp": timestamp, **dict(zip(names, values))}


def parse_mac(text: str) -> bytes:
    parts = text.split(":")
    mac = bytes(int(p, 16) for p in parts)
    if len(parts) != 6 or len(mac) != 6 or not any(mac) or mac[0] & 1:
        raise argparse.ArgumentTypeError(f"not an interface MAC address: {text}")
    return mac


def parse_str(text: str) -> bytes:
    raw = text.encode("ascii")
    if len(raw) > 31:
        raise argparse.ArgumentTypeError(f"longer than 31 chars: {text}")
    return raw


def parse_hex(size: int):
    def parse(text: str) -> bytes:
        raw = bytes.fromhex(text)
        if len(raw) != size:
            raise argparse.ArgumentTypeError(f"{size} bytes of hex expected")
        return raw

    return parse


def signed_data(chip_id: bytes, index: int, block: bytes) -> bytes:
    return chip_id + f"-otp{index}-".encode() + block


def sig_from_der(der: bytes) -> bytes:
    from cryptography.hazmat.primitives.asymmetric.utils import decode_dss_signature

    r, s = decode_dss_signature(der)
    return r.to_bytes(32, "big") + s.to_bytes(32, "big")


def sig_to_der(sig: bytes) -> bytes:
    from cryptography.hazmat.primitives.asymmetric.utils import encode_dss_signature

    return encode_dss_signature(int.from_bytes(sig[:32], "big"), int.from_bytes(sig[32:], "big"))


def read(path: str) -> bytes:
    with open(path, "rb") as f:
        return f.read()


def write(path: str, data: bytes):
    with open(path, "wb") as f:
        f.write(data)
    print(f"{path}: {len(data)} bytes")


def cmd_generate_key(args):
    from cryptography.hazmat.primitives import serialization
    from cryptography.hazmat.primitives.asymmetric import ec

    key = ec.generate_private_key(ec.SECP256R1())
    pem = key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.NoEncryption())
    write(args.output, pem)
    point = key.public_key().public_bytes(serialization.Encoding.X962, serialization.PublicFormat.UncompressedPoint)
    print("public key (X || Y), for the firmware:", point[1:].hex())


def cmd_public_key(args):
    from cryptography.hazmat.primitives import serialization

    key = serialization.load_pem_private_key(read(args.key), password=None)
    point = key.public_key().public_bytes(serialization.Encoding.X962, serialization.PublicFormat.UncompressedPoint)
    print(point[1:].hex())


def cmd_create_1(args):
    write(args.output, pack(1, args.timestamp, args.hw_version, args.target, args.body, args.connect, args.chip_id))


def cmd_create_2(args):
    macs = b"".join(getattr(args, name) for name in MAC_NAMES)
    region = args.region.encode("ascii")
    if len(region) != 2:
        raise SystemExit("--region is two letters")
    write(
        args.output,
        pack(2, args.timestamp, macs, args.name, args.model, args.color, region, args.cpu_id, args.ram_gb, args.ufs_gb, args.display),
    )


def cmd_sign(args):
    from cryptography.hazmat.primitives import hashes, serialization
    from cryptography.hazmat.primitives.asymmetric import ec

    key = serialization.load_pem_private_key(read(args.key), password=None)
    b1, b2 = read(args.otp1), read(args.otp2)
    chip_id = unpack(b1, 1)["chip_id"]
    unpack(b2, 2)
    sigs = [sig_from_der(key.sign(signed_data(chip_id, i, b), ec.ECDSA(hashes.SHA256()))) for i, b in ((1, b1), (2, b2))]
    write(args.output, pack(3, args.timestamp, args.key_id, *sigs))


def cmd_verify(args):
    from cryptography.exceptions import InvalidSignature
    from cryptography.hazmat.primitives import hashes
    from cryptography.hazmat.primitives.asymmetric import ec

    b1, b2, b3 = read(args.otp1), read(args.otp2), read(args.otp3)
    chip_id = unpack(b1, 1)["chip_id"]
    unpack(b2, 2)
    f3 = unpack(b3, 3)
    point = bytes.fromhex(args.public_key)
    pub = ec.EllipticCurvePublicKey.from_encoded_point(ec.SECP256R1(), b"\x04" + point)
    ok = True
    for i, b, sig in ((1, b1, f3["block1_signature"]), (2, b2, f3["block2_signature"])):
        try:
            pub.verify(sig_to_der(sig), signed_data(chip_id, i, b), ec.ECDSA(hashes.SHA256()))
            print(f"block {i}: signature verifies (key id {f3['key_id']})")
        except InvalidSignature:
            print(f"block {i}: signature does NOT verify")
            ok = False
    sys.exit(0 if ok else 1)


def cmd_load(args):
    data = read(args.file)
    (index,) = struct.unpack_from("<H", data, 4)
    for name, value in unpack(data, index).items():
        if name == "macs":
            for i, mac_name in enumerate(MAC_NAMES):
                print(f"{mac_name:14} {value[i * 6:i * 6 + 6].hex(':')}")
            continue
        if isinstance(value, bytes):
            value = value.rstrip(b"\0").decode("ascii") if name in ("name", "model", "region") else value.hex()
        print(f"{name:14} {value}")


def cmd_hex(args):
    print(read(args.file).hex())


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)

    s = sub.add_parser("generate-key", help="a P-256 key pair: private PEM to a file, public key printed")
    s.add_argument("-o", "--output", required=True)
    s.set_defaults(func=cmd_generate_key)

    s = sub.add_parser("public-key", help="the public key (X || Y hex) of a private PEM")
    s.add_argument("--key", required=True)
    s.set_defaults(func=cmd_public_key)

    s = sub.add_parser("create-1", help="block 1, the board")
    s.add_argument("--hw-version", type=int, required=True)
    s.add_argument("--target", type=int, required=True, help="FW_TARGET number: f2 is 2")
    s.add_argument("--body", type=int, required=True)
    s.add_argument("--connect", type=int, required=True)
    s.add_argument("--chip-id", type=parse_hex(8), required=True, help="as `otp chipid` prints it")
    s.add_argument("--timestamp", type=int, default=int(time.time()))
    s.add_argument("-o", "--output", required=True)
    s.set_defaults(func=cmd_create_1)

    s = sub.add_parser("create-2", help="block 2, the device")
    for name in MAC_NAMES:
        s.add_argument(f"--{name.replace('_', '-')}", dest=name, type=parse_mac, required=True)
    s.add_argument("--name", type=parse_str, required=True)
    s.add_argument("--model", type=parse_str, required=True)
    s.add_argument("--color", type=int, required=True)
    s.add_argument("--region", required=True, help="two letters")
    s.add_argument("--cpu-id", type=parse_hex(16), required=True)
    s.add_argument("--ram-gb", type=int, required=True)
    s.add_argument("--ufs-gb", type=int, required=True)
    s.add_argument("--display", type=int, required=True)
    s.add_argument("--timestamp", type=int, default=int(time.time()))
    s.add_argument("-o", "--output", required=True)
    s.set_defaults(func=cmd_create_2)

    s = sub.add_parser("sign", help="block 3: signatures of blocks 1 and 2")
    s.add_argument("--key", required=True, help="private key PEM")
    s.add_argument("--key-id", type=int, required=True, help="the key's number in the firmware")
    s.add_argument("--otp1", required=True)
    s.add_argument("--otp2", required=True)
    s.add_argument("--timestamp", type=int, default=int(time.time()))
    s.add_argument("-o", "--output", required=True)
    s.set_defaults(func=cmd_sign)

    s = sub.add_parser("verify", help="check block 3's signatures with a public key")
    s.add_argument("--otp1", required=True)
    s.add_argument("--otp2", required=True)
    s.add_argument("--otp3", required=True)
    s.add_argument("--public-key", required=True, help="X || Y, 64 bytes of hex")
    s.set_defaults(func=cmd_verify)

    s = sub.add_parser("load", help="print a block file")
    s.add_argument("file")
    s.set_defaults(func=cmd_load)

    s = sub.add_parser("hex", help="a block file as hex, for `otp program`")
    s.add_argument("file")
    s.set_defaults(func=cmd_hex)

    args = p.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
