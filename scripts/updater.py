"""Flipper One MCU firmware updater over I2C."""

import argparse
import struct
import sys
import time
from enum import IntEnum
from pathlib import Path

from periphery import I2C

MCU_ADDR = 0x69

BLOCK_SIZE = 256

ERASE_TIMEOUT_S = 120
BLOCK_TIMEOUT_S = 5
FINALIZE_TIMEOUT_S = 5
POLL_INTERVAL_S = 0.01


class Reg(IntEnum):
    STATUS = 0xF000
    FW_VERSION = 0xF002
    FW_COMMAND = 0xF004
    FW_CRC = 0xF020
    FW_BLOCKS = 0xF022
    FW_DATA = 0xF040


# Keep in sync with UpdaterState/UpdaterError/UpdaterCommand from the MCU firmware
class UpdaterState(IntEnum):
    IDLE = 0
    RUNNING = 1
    BUSY = 2
    DONE = 3
    ERROR = 4


class UpdaterError(IntEnum):
    NONE = 0
    NOT_STARTED = 1
    SIZE_ERROR = 2
    BLOCK_CRC_ERROR = 3
    BLOCK_INDEX_ERROR = 4
    FW_CRC_ERROR = 5
    FW_VERSION_ERROR = 6
    PROTOCOL_ERROR = 7
    FW_WRITE_ERROR = 8
    FW_INVALIDATE_ERROR = 9
    FW_PARAMS_ERROR = 10


class UpdaterCommand(IntEnum):
    NONE = 0
    START = 1
    ABORT = 2


class UpdateError(Exception):
    pass


def _build_crc16_table(polynomial):
    table = []
    for byte in range(256):
        crc = 0
        c = byte
        for _ in range(8):
            if (crc ^ c) & 0x0001:
                crc = (crc >> 1) ^ polynomial
            else:
                crc >>= 1
            c >>= 1
        table.append(crc)
    return tuple(table)


_CRC16_TABLE = _build_crc16_table(0xA001)


def crc16_modbus(data):
    """CRC-16/MODBUS (reflected 0xA001, init 0xFFFF), matching the MCU's updater_crc16."""
    crc = 0xFFFF
    for byte in data:
        crc = (crc >> 8) ^ _CRC16_TABLE[(crc ^ byte) & 0xFF]
    return crc

class McuI2C:
    def __init__(self, bus="/dev/i2c-0"):
        self.i2c = I2C(bus)

    def close(self):
        self.i2c.close()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.close()

    def read_register(self, address):
        read_msg = I2C.Message(bytes(2), read=True)
        self.i2c.transfer(MCU_ADDR, [I2C.Message(struct.pack(">H", address)), read_msg])
        return struct.unpack("<H", read_msg.data)[0]

    def write_register(self, address, value):
        # Little-endian value: the MCU fires the write callback on the high byte
        self.i2c.transfer(MCU_ADDR, [I2C.Message(struct.pack(">H", address) + struct.pack("<H", value))])

    def write_block(self, address, buf):
        self.i2c.transfer(MCU_ADDR, [I2C.Message(struct.pack(">H", address) + bytes(buf))])

def get_state(mcu) -> tuple[UpdaterState, UpdaterError]:
    status = mcu.read_register(Reg.STATUS)
    return UpdaterState(status & 0xFF), UpdaterError(status >> 8)


def send_command(mcu, command: UpdaterCommand) -> None:
    mcu.write_register(Reg.FW_COMMAND, command)


def wait_for_state(mcu, target_state, timeout_s) -> tuple[UpdaterState, UpdaterError]:
    """Poll until target_state or ERROR; raise UpdateError on timeout."""
    deadline = time.monotonic() + timeout_s
    while True:
        state, error = get_state(mcu)
        if state in (target_state, UpdaterState.ERROR):
            return state, error
        if time.monotonic() >= deadline:
            raise UpdateError(f"Timeout waiting for state {target_state.name}")
        time.sleep(POLL_INTERVAL_S)


def load_firmware(path: Path) -> tuple[bytes, int, int]:
    data = path.read_bytes()
    data += b"\x00" * (-len(data) % BLOCK_SIZE)
    return data, len(data) // BLOCK_SIZE, crc16_modbus(data)


def iter_blocks(firmware: bytes):
    # <256sHH: 256-byte payload + little-endian block index + block CRC
    for index, offset in enumerate(range(0, len(firmware), BLOCK_SIZE)):
        block = firmware[offset:offset + BLOCK_SIZE]
        yield struct.pack(f"<{BLOCK_SIZE}sHH", block, index, crc16_modbus(block))


def run_update(mcu, firmware: bytes, block_count: int) -> None:
    mcu.write_register(Reg.FW_CRC, crc16_modbus(firmware))
    mcu.write_register(Reg.FW_BLOCKS, block_count)
    send_command(mcu, UpdaterCommand.START)

    print("Erasing flash area...")
    state, error = wait_for_state(mcu, UpdaterState.RUNNING, ERASE_TIMEOUT_S)
    if state != UpdaterState.RUNNING:
        raise UpdateError(f"Failed to start update: state={state.name}, error={error.name}")

    start_time = time.perf_counter()
    for block_idx, frame in enumerate(iter_blocks(firmware)):
        print(f"Sending block {block_idx}/{block_count}")
        state, error = wait_for_state(mcu, UpdaterState.RUNNING, BLOCK_TIMEOUT_S)
        if state != UpdaterState.RUNNING:
            raise UpdateError(f"Block {block_idx}: state={state.name}, error={error.name}")
        mcu.write_block(Reg.FW_DATA, frame)
    elapsed_s = time.perf_counter() - start_time

    state, error = wait_for_state(mcu, UpdaterState.DONE, FINALIZE_TIMEOUT_S)
    if state != UpdaterState.DONE:
        raise UpdateError(f"Finalize failed: state={state.name}, error={error.name}")

    print(f"State: {state.name}")
    print(f"Written in {elapsed_s:.2f}s ({len(firmware) * 8 / elapsed_s / 1024:.2f} Kbit/s)")


def main() -> int:
    parser = argparse.ArgumentParser(description="Flipper One MCU firmware updater")
    parser.add_argument("--bus", default="/dev/i2c-0", help="I2C bus device (default: /dev/i2c-0)")
    parser.add_argument("--file", dest="file_path", type=Path, required=True, help="Firmware file to send to the MCU")
    args = parser.parse_args()

    firmware, block_count, fw_crc = load_firmware(args.file_path)
    print(f"Firmware size: {len(firmware)} bytes, {block_count} blocks, CRC: 0x{fw_crc:04X}")

    try:
        with McuI2C(args.bus) as mcu:
            state, error = get_state(mcu)
            fw_version = mcu.read_register(Reg.FW_VERSION)
            print(f"State: {state.name}, Current FW version: 0x{fw_version:04X}")
            if state != UpdaterState.IDLE:
                print("Aborting previous update")
                send_command(mcu, UpdaterCommand.ABORT)

            run_update(mcu, firmware, block_count)
    except UpdateError as exc:
        print(f"Update failed: {exc}")
        return 1

    print("Update complete. Reset MCU to start new firmware.")
    return 0


if __name__ == "__main__":
    sys.exit(main())