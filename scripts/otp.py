#!/usr/bin/env python3
"""Build, sign and check the Flipper One MCU OTP blocks.

The layout is targets/f1/furi_hal/furi_hal_otp.h; the commands follow
BusyBar's bsbotp.py. A block is printed with its raw hex, or saved to a file
with -o, and written on the device with `otp program OTP<n> <hex>`:

    otp.py create-otp1 --hw-version 1 --target 2 --body 1 --connect 1 --chip-id <rp2350_hardware_uid> -o otp1.bin
    otp.py create-otp2 --eth0 ... --name ... -o otp2.bin
    otp.py create-otp3 --key key.pem --key-id 0 --otp1 otp1.bin --otp2 otp2.bin -o otp3.bin
    otp.py load otp1.bin     # its fields and the hex for `otp program OTP1`

Keep the files: a write cut short (power, a reset) is finished by sending the
same block again, byte for byte. A block made anew differs (its timestamp at
least), and the device refuses it. So -o does not overwrite a file.

Keys and signatures need the `cryptography` package.
"""

import argparse
import os
import struct
import sys
import time

MAGIC = 0x544F3146  # "F1OT"
HEADER = "<IHHQ"  # magic, index, version, timestamp
VERSIONS = {1: 1, 2: 1, 3: 1}  # the layout version of each block the firmware knows
MAC_NAMES = ["eth0", "eth1", "usb", "wifi_2g4_sta", "wifi_5g_sta", "wifi_2g4_ap", "wifi_5g_ap", "bt"]

# Fields after the header, before the CRC (struct format, names)
LAYOUTS = {
    1: ("<HHHH8s", ["hw_version", "target", "body", "connect", "chip_id"]),
    2: ("<48s32s32sH2s16sIIH", ["macs", "name", "model", "color", "region", "cpu_id", "ram_gb", "ufs_gb", "display"]),
    3: ("<H64s64s", ["key_id", "otp1_signature", "otp2_signature"]),
}
SIZES = {1: 34, 2: 160, 3: 148}


def crc16(data: bytes) -> int:
    """CRC-16/CCITT-FALSE"""
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def pack(index: int, version: int, timestamp: int, *fields) -> bytes:
    body = struct.pack(HEADER, MAGIC, index, version, timestamp) + struct.pack(LAYOUTS[index][0], *fields)
    # The CRC covers everything but the magic and itself
    data = body + struct.pack("<H", crc16(body[4:]))
    assert len(data) == SIZES[index], (index, len(data))
    return data


def unpack(data: bytes, index: int = None) -> dict:
    if len(data) < struct.calcsize(HEADER):
        raise ValueError("too short for an OTP block")
    magic, idx, version, timestamp = struct.unpack_from(HEADER, data)
    if magic != MAGIC:
        raise ValueError(f"magic {magic:#010x}, expected {MAGIC:#010x}")
    if idx not in SIZES or (index is not None and idx != index):
        raise ValueError(f"index {idx}" + (f", expected {index}" if index is not None else ""))
    if len(data) != SIZES[idx]:
        raise ValueError(f"OTP{idx} is {SIZES[idx]} bytes, got {len(data)}")
    if version != VERSIONS[idx]:
        raise ValueError(f"OTP{idx} version {version}, the firmware knows {VERSIONS[idx]}")
    (crc,) = struct.unpack_from("<H", data, len(data) - 2)
    if crc != crc16(data[4:-2]):
        raise ValueError(f"OTP{idx}: CRC mismatch")
    fmt, names = LAYOUTS[idx]
    values = struct.unpack_from(fmt, data, struct.calcsize(HEADER))
    return {"index": idx, "version": version, "timestamp": timestamp, **dict(zip(names, values))}


def show(data: bytes, output: str = None):
    """Print a block's fields; save it to output, or print its raw hex"""
    fields = unpack(data)
    print(f"OTP{fields['index']} data:")
    for name, value in fields.items():
        if name == "index":
            continue
        if name == "macs":
            for i, mac_name in enumerate(MAC_NAMES):
                print(f"  {mac_name:14} {value[i * 6:i * 6 + 6].hex(':')}")
            continue
        if isinstance(value, bytes):
            value = value.rstrip(b"\0").decode("ascii") if name in ("name", "model", "region") else value.hex()
        print(f"  {name:14} {value}")
    if output:
        if os.path.exists(output):
            raise SystemExit(f"{output} exists: send that one, or remove it to make a new block")
        with open(output, "wb") as f:
            f.write(data)
        print(f"OTP{fields['index']} saved to: {output}")
    else:
        print(f"  Raw: {data.hex()}")


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


def read(path: str) -> bytes:
    with open(path, "rb") as f:
        return f.read()


def public_bytes(private_key) -> bytes:
    """X || Y, 64 bytes, as the firmware carries it"""
    numbers = private_key.public_key().public_numbers()
    return numbers.x.to_bytes(32, "big") + numbers.y.to_bytes(32, "big")


def cmd_generate_key(args):
    from cryptography.hazmat.primitives import serialization
    from cryptography.hazmat.primitives.asymmetric import ec

    key = ec.generate_private_key(ec.SECP256R1())
    pem = key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.NoEncryption())
    with open(args.output, "wb") as f:
        f.write(pem)
    print(f"Private key saved to: {args.output}")
    if args.public:
        with open(args.public, "wb") as f:
            f.write(public_bytes(key))
        print(f"Public key (raw) saved to: {args.public}")
    print(f"Public key hex: {public_bytes(key).hex()}")


def cmd_create_otp1(args):
    show(pack(1, args.version, args.timestamp, args.hw_version, args.target, args.body, args.connect, args.chip_id), args.output)


def cmd_create_otp2(args):
    macs = b"".join(getattr(args, name) for name in MAC_NAMES)
    region = args.region.encode("ascii")
    if len(region) != 2:
        raise SystemExit("--region is two letters")
    fields = (macs, args.name, args.model, args.color, region, args.cpu_id, args.ram_gb, args.ufs_gb, args.display)
    show(pack(2, args.version, args.timestamp, *fields), args.output)


def cmd_create_otp3(args):
    from cryptography.hazmat.primitives import hashes, serialization
    from cryptography.hazmat.primitives.asymmetric import ec
    from cryptography.hazmat.primitives.asymmetric.utils import decode_dss_signature

    key = serialization.load_pem_private_key(read(args.key), password=None)
    otp1, otp2 = read(args.otp1), read(args.otp2)
    chip_id = unpack(otp1, 1)["chip_id"]
    unpack(otp2, 2)
    sigs = []
    for index, block in ((1, otp1), (2, otp2)):
        # Deterministic (RFC 6979): the same blocks and key give the same OTP3
        ecdsa = ec.ECDSA(hashes.SHA256(), deterministic_signing=True)
        r, s = decode_dss_signature(key.sign(signed_data(chip_id, index, block), ecdsa))
        sigs.append(r.to_bytes(32, "big") + s.to_bytes(32, "big"))
    show(pack(3, args.version, args.timestamp, args.key_id, *sigs), args.output)


def cmd_verify(args):
    from cryptography.exceptions import InvalidSignature
    from cryptography.hazmat.primitives import hashes
    from cryptography.hazmat.primitives.asymmetric import ec
    from cryptography.hazmat.primitives.asymmetric.utils import encode_dss_signature

    otp1, otp2, otp3 = read(args.otp1), read(args.otp2), read(args.otp3)
    chip_id = unpack(otp1, 1)["chip_id"]
    unpack(otp2, 2)
    fields = unpack(otp3, 3)
    point = read(args.public_key)
    if len(point) != 64:
        raise SystemExit("--public-key: a raw X || Y file of 64 bytes, as generate-key --public writes it")
    pub = ec.EllipticCurvePublicKey.from_encoded_point(ec.SECP256R1(), b"\x04" + point)
    ok = True
    for index, block, sig in ((1, otp1, fields["otp1_signature"]), (2, otp2, fields["otp2_signature"])):
        der = encode_dss_signature(int.from_bytes(sig[:32], "big"), int.from_bytes(sig[32:], "big"))
        try:
            pub.verify(der, signed_data(chip_id, index, block), ec.ECDSA(hashes.SHA256()))
            print(f"OTP{index} signature: valid (key id {fields['key_id']})")
        except InvalidSignature:
            print(f"OTP{index} signature: INVALID")
            ok = False
    sys.exit(0 if ok else 1)


def cmd_load(args):
    show(read(args.file))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)
    now = int(time.time())

    s = sub.add_parser("generate-key", help="a P-256 key pair: private PEM to a file, public key printed")
    s.add_argument("-o", "--output", required=True, help="private key file (PEM)")
    s.add_argument("--public", help="also save the public key, raw X || Y")
    s.set_defaults(func=cmd_generate_key)

    s = sub.add_parser("create-otp1", help="OTP1, the board")
    s.add_argument("-o", "--output", help="output file (if not given, prints hex data)")
    s.add_argument("--version", type=int, default=VERSIONS[1], help=f"OTP1 layout version (default: {VERSIONS[1]})")
    s.add_argument("--timestamp", type=int, default=now, help="(default: now)")
    s.add_argument("--hw-version", type=int, required=True)
    s.add_argument("--target", type=int, required=True, help="FW_TARGET number: f2 is 2")
    s.add_argument("--body", type=int, required=True)
    s.add_argument("--connect", type=int, required=True)
    s.add_argument("--chip-id", type=parse_hex(8), required=True, help="rp2350_hardware_uid, as `device_info` prints it")
    s.set_defaults(func=cmd_create_otp1)

    s = sub.add_parser("create-otp2", help="OTP2, the device")
    s.add_argument("-o", "--output", help="output file (if not given, prints hex data)")
    s.add_argument("--version", type=int, default=VERSIONS[2], help=f"OTP2 layout version (default: {VERSIONS[2]})")
    s.add_argument("--timestamp", type=int, default=now, help="(default: now)")
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
    s.set_defaults(func=cmd_create_otp2)

    s = sub.add_parser("create-otp3", help="OTP3, the signatures of OTP1 and OTP2")
    s.add_argument("-o", "--output", help="output file (if not given, prints hex data)")
    s.add_argument("--version", type=int, default=VERSIONS[3], help=f"OTP3 layout version (default: {VERSIONS[3]})")
    s.add_argument("--timestamp", type=int, default=now, help="(default: now)")
    s.add_argument("--key", required=True, help="private key file (PEM)")
    s.add_argument("--key-id", type=int, required=True, help="the key's number in the firmware")
    s.add_argument("--otp1", required=True, help="OTP1 data file")
    s.add_argument("--otp2", required=True, help="OTP2 data file")
    s.set_defaults(func=cmd_create_otp3)

    s = sub.add_parser("verify", help="check OTP3's signatures")
    s.add_argument("--otp1", required=True, help="OTP1 data file")
    s.add_argument("--otp2", required=True, help="OTP2 data file")
    s.add_argument("--otp3", required=True, help="OTP3 data file")
    s.add_argument("--public-key", required=True, help="public key file, raw X || Y")
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
