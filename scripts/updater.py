"""Flipper One MCU firmware updater over I2C."""

import argparse
import binascii
import signal
import struct
import sys
import time
from enum import IntEnum
from pathlib import Path

from periphery import I2C, I2CError

MCU_ADDR = 0x69

BLOCK_SIZE = 256

ERASE_TIMEOUT_S = 120
BLOCK_TIMEOUT_S = 5
FINALIZE_TIMEOUT_S = 5
POLL_INTERVAL_S = 0.01
TRANSFER_TIMEOUT_S = 1.0
UPDATE_ATTEMPTS = 3


class Reg(IntEnum):
    STATUS = 0xF000
    FW_VERSION = 0xF002
    FW_COMMAND = 0xF004
    FW_CRC = 0xF020
    FW_BLOCKS = 0xF022
    FW_DATA = 0xF040
    SYSTEM_RESET = 0x0090
    SYSTEM_RESET_MODE = 0x0092
    SYSTEM_RESET_DELAY = 0x0094


SYSTEM_RESET_MAGIC = 0x5A52


class ResetMode(IntEnum):
    DISABLED = 0
    RESET_MCU = 1
    RESET_MCU_AND_POWER = 2
    RESET_MCU_IMMEDIATELY = 3


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


def crc16_xmodem(data):
    """CRC-16/XMODEM (CCITT 0x1021, init 0x0000), matching the MCU's updater_crc16."""
    return binascii.crc_hqx(data, 0)


def _addr16(address):
    return struct.pack(">H", address)


def _transfer_timeout_handler(signum, frame):
    raise TimeoutError("I2C transfer timed out")


class McuI2C:
    def __init__(self, bus="/dev/i2c-0"):
        self.i2c = I2C(bus)

    def close(self):
        self.i2c.close()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.close()

    def _transfer(self, messages):
        previous = signal.signal(signal.SIGALRM, _transfer_timeout_handler)
        signal.setitimer(signal.ITIMER_REAL, TRANSFER_TIMEOUT_S)
        try:
            self.i2c.transfer(MCU_ADDR, messages)
        except (I2CError, OSError, TimeoutError) as exc:
            raise UpdateError(f"I2C transfer failed: {exc}") from exc
        finally:
            signal.setitimer(signal.ITIMER_REAL, 0)
            signal.signal(signal.SIGALRM, previous)

    def read_register(self, address):
        read_msg = I2C.Message(bytes(2), read=True)
        self._transfer([I2C.Message(_addr16(address)), read_msg])
        return struct.unpack("<H", read_msg.data)[0]

    def write_register(self, address, value):
        # Little-endian value: the MCU fires the write callback on the high byte
        self._transfer([I2C.Message(_addr16(address) + struct.pack("<H", value))])

    def write_block(self, address, buf):
        self._transfer([I2C.Message(_addr16(address) + bytes(buf))])


def get_state(mcu) -> tuple[UpdaterState, UpdaterError] | None:
    status = mcu.read_register(Reg.STATUS)
    try:
        return UpdaterState(status & 0xFF), UpdaterError(status >> 8)
    except ValueError:
        # Torn read: the collapsed-transfer recovery can serve dummy bytes
        return None


def get_state_valid(mcu) -> tuple[UpdaterState, UpdaterError]:
    deadline = time.monotonic() + 1.0
    while True:
        result = get_state(mcu)
        if result is not None:
            return result
        if time.monotonic() >= deadline:
            raise UpdateError("Invalid status register value")
        time.sleep(POLL_INTERVAL_S)


def send_command(mcu, command: UpdaterCommand) -> None:
    mcu.write_register(Reg.FW_COMMAND, command)


def ensure_idle(mcu) -> None:
    state, error = get_state_valid(mcu)
    if state != UpdaterState.IDLE:
        print("Aborting previous update")
        send_command(mcu, UpdaterCommand.ABORT)
        state, error = wait_for_state(mcu, UpdaterState.IDLE, ERASE_TIMEOUT_S)
        if state != UpdaterState.IDLE:
            raise UpdateError(f"Failed to abort previous update: state={state.name}, error={error.name}")


def wait_for_state(mcu, target_state, timeout_s) -> tuple[UpdaterState, UpdaterError]:
    """Poll until target_state or ERROR; raise UpdateError on timeout."""
    deadline = time.monotonic() + timeout_s
    while True:
        result = get_state(mcu)
        if result is not None:
            state, error = result
            if state in (target_state, UpdaterState.ERROR):
                return state, error
        if time.monotonic() >= deadline:
            raise UpdateError(f"Timeout waiting for state {target_state.name}")
        time.sleep(POLL_INTERVAL_S)


def load_firmware(path: Path) -> tuple[bytes, int, int]:
    data = path.read_bytes()
    data += b"\x00" * (-len(data) % BLOCK_SIZE)
    return data, len(data) // BLOCK_SIZE, crc16_xmodem(data)


def iter_blocks(firmware: bytes):
    # <256sHH: 256-byte payload + little-endian block index + block CRC
    for index, offset in enumerate(range(0, len(firmware), BLOCK_SIZE)):
        block = firmware[offset:offset + BLOCK_SIZE]
        yield struct.pack(f"<{BLOCK_SIZE}sHH", block, index, crc16_xmodem(block))


def run_update(mcu, firmware: bytes, block_count: int) -> None:
    for attempt in (1, 2):
        mcu.write_register(Reg.FW_CRC, crc16_xmodem(firmware))
        mcu.write_register(Reg.FW_BLOCKS, block_count)
        send_command(mcu, UpdaterCommand.START)

        print("Erasing flash area...")
        state, error = wait_for_state(mcu, UpdaterState.RUNNING, ERASE_TIMEOUT_S)
        if state != UpdaterState.RUNNING:
            raise UpdateError(f"Failed to start update: state={state.name}, error={error.name}")

        start_time = time.perf_counter()
        retry = False
        for block_idx, frame in enumerate(iter_blocks(firmware)):
            print(f"Sending block {block_idx}/{block_count}")
            state, error = wait_for_state(mcu, UpdaterState.RUNNING, BLOCK_TIMEOUT_S)
            if state != UpdaterState.RUNNING:
                # A partial/garbage frame can park the MCU in BUSY: abort and
                # re-run the whole update once.
                print(f"Block {block_idx}: state={state.name}, error={error.name}, aborting")
                send_command(mcu, UpdaterCommand.ABORT)
                wait_for_state(mcu, UpdaterState.IDLE, ERASE_TIMEOUT_S)
                retry = True
                break
            mcu.write_block(Reg.FW_DATA, frame)
        if retry:
            if attempt == 2:
                raise UpdateError("Update failed after retry")
            continue
        elapsed_s = time.perf_counter() - start_time

        state, error = wait_for_state(mcu, UpdaterState.DONE, FINALIZE_TIMEOUT_S)
        if state != UpdaterState.DONE:
            raise UpdateError(f"Finalize failed: state={state.name}, error={error.name}")

        print(f"State: {state.name}")
        print(f"Written in {elapsed_s:.2f}s ({len(firmware) * 8 / elapsed_s / 1024:.2f} Kbit/s)")
        return


def main() -> int:
    parser = argparse.ArgumentParser(description="Flipper One MCU firmware updater")
    parser.add_argument("--bus", default="/dev/i2c-0", help="I2C bus device (default: /dev/i2c-0)")
    parser.add_argument("--file", dest="file_path", type=Path, required=True, help="Firmware file to send to the MCU")
    parser.add_argument(
        "--reset",
        action="store_true",
        help="Reset the MCU immediately after the update",
    )
    parser.add_argument(
        "--reset-mode",
        type=int,
        choices=[mode.value for mode in ResetMode],
        default=None,
        help="MCU response to the host shutting down (default: none, the MCU arms reset-on-host-shutdown itself after a successful update)",
    )
    parser.add_argument(
        "--reset-delay",
        type=int,
        default=0,
        help="Delay in 100ms units before the armed reset fires (default: 0 = immediate)",
    )
    args = parser.parse_args()

    firmware, block_count, fw_crc = load_firmware(args.file_path)
    print(f"Firmware size: {len(firmware)} bytes, {block_count} blocks, CRC: 0x{fw_crc:04X}")

    try:
        with McuI2C(args.bus) as mcu:
            state, error = get_state_valid(mcu)
            fw_version = mcu.read_register(Reg.FW_VERSION)
            print(f"State: {state.name}, Current FW version: 0x{fw_version:04X}")
            ensure_idle(mcu)

            for attempt in range(1, UPDATE_ATTEMPTS + 1):
                try:
                    run_update(mcu, firmware, block_count)
                    break
                except UpdateError as exc:
                    print(f"Update attempt {attempt}/{UPDATE_ATTEMPTS} failed: {exc}")
                    ensure_idle(mcu)
                if attempt == UPDATE_ATTEMPTS:
                    raise UpdateError(f"Update failed after {UPDATE_ATTEMPTS} attempts")

            if args.reset:
                mcu.write_register(Reg.SYSTEM_RESET, SYSTEM_RESET_MAGIC)
                print("Reset command sent to the MCU.")
            elif args.reset_mode is not None:
                mcu.write_register(Reg.SYSTEM_RESET_DELAY, args.reset_delay)
                mcu.write_register(Reg.SYSTEM_RESET_MODE, args.reset_mode)
                print(f"Reset response set: {ResetMode(args.reset_mode).name}, delay: {args.reset_delay * 100} ms")
            else:
                print("Update complete. The image is staged in the inactive partition; the MCU switches to it only on a flash-update boot.")
    except UpdateError as exc:
        print(f"Update failed: {exc}")
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
