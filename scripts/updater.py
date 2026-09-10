from periphery import I2C
from enum import Enum
import time
import argparse

MCU_ADDR = 0x69

REG_STATUS = 0xF000
REG_FW_VERSION = 0xF002
REG_FW_COMMAND = 0xF004

REG_FW_CRC = 0xF020
REG_FW_BLOCKS = 0xF022

REG_FW_DATA = 0xF040

class UpdaterStatus(Enum):
    IDLE = 0
    RUNNING = 1
    BUSY = 2
    DONE = 3
    ERROR = 4

class UpdaterError(Enum):
    NONE = 0
    NOT_STARTED = 1
    SIZE_ERROR = 2
    BLOCK_CRC_ERROR = 3
    BLOCK_INDEX_ERROR = 4
    FW_CRC_ERROR = 5
    FW_VERSION_ERROR = 6

class UpdaterCommand(Enum):
    NONE = 0
    START = 1
    ABORT = 2

class Crc16:
    def __init__(self, polynomial=0xA001):
        self.crc_table = self._init_crc_table(polynomial)

    def _init_crc_table(self, polynomial):
        crc_table = []
        for byte in range(256):
            crc = 0
            c = byte
            for _ in range(8):
                if (crc ^ c) & 0x0001:
                    crc = (crc >> 1) ^ polynomial
                else:
                    crc >>= 1
                c >>= 1
            crc_table.append(crc)
        return crc_table

    def compute(self, data):
        crc = 0xFFFF
        for byte in data:
            crc = (crc >> 8) ^ self.crc_table[(crc ^ byte) & 0xFF]
        return crc

class McuI2C:
    def __init__(self, bus="/dev/i2c-0"):
        self.i2c = I2C(bus)
    def __del__(self):
        self.i2c.close()

    def read_register(self, register_address):
        addr_high = (register_address >> 8) & 0xFF
        addr_low = register_address & 0xFF
    
        write_msg = I2C.Message([addr_high, addr_low])
        read_msg = I2C.Message([0x00, 0x00], read=True)
    
        self.i2c.transfer(MCU_ADDR, [write_msg, read_msg])
    
        read_word = (read_msg.data[1] << 8) | read_msg.data[0]
        return read_word

    def write_register(self, register_address, value):
        addr_high = (register_address >> 8) & 0xFF
        addr_low = register_address & 0xFF

        value_high = value & 0xFF
        value_low = (value >> 8) & 0xFF
    
        write_msg = I2C.Message([addr_high, addr_low, value_high, value_low])
        self.i2c.transfer(MCU_ADDR, [write_msg])
    
    def write_block(self, register_address, buf):
        addr_high = (register_address >> 8) & 0xFF
        addr_low = register_address & 0xFF
    
        write_msg = I2C.Message([addr_high, addr_low] + list(buf))
        self.i2c.transfer(MCU_ADDR, [write_msg])

def get_state(mcu) -> tuple[UpdaterStatus, UpdaterError]:
    reg_status = mcu.read_register(REG_STATUS)
    return UpdaterStatus(reg_status & 0xFF), UpdaterError((reg_status >> 8) & 0xFF)

def send_command(mcu, command: UpdaterCommand):
    mcu.write_register(REG_FW_COMMAND, int(command.value))

def main():
    crc16 = Crc16()
    mcu = McuI2C("/dev/i2c-0")

    parser = argparse.ArgumentParser(description="MCU updater demo")
    parser.add_argument(
        "--file",
        dest="file_path",
        type=str,
        required=True,
        help="Path to the firmware file to send to the MCU",
    )
    args = parser.parse_args()

    state, error = get_state(mcu)
    reg_fw_version = mcu.read_register(REG_FW_VERSION)
    print(f"State: {state.name}, Current FW version: 0x{reg_fw_version:04X}")
    if state != UpdaterStatus.IDLE:
        print("Aborting previous update")
        send_command(mcu, UpdaterCommand.ABORT)

    with open(args.file_path, "rb") as f:
        firmware_data = f.read()

    # pad firmware_data to a multiple of 256 bytes
    firmware_data += bytes((256 - len(firmware_data) % 256) % 256)
    block_count = (len(firmware_data) + 255) // 256
    print(f"Firmware size: {len(firmware_data)} bytes, {block_count} blocks")
    
    fw_crc = crc16.compute(firmware_data)

    mcu.write_register(REG_FW_CRC, fw_crc)
    mcu.write_register(REG_FW_BLOCKS, block_count)
    send_command(mcu, UpdaterCommand.START)  # Start update

    print("Erasing flash area...")

    state, error = get_state(mcu)
    while state != UpdaterStatus.RUNNING:
        time.sleep(1)
        state, error = get_state(mcu)
        print(f"State: {state.name}")
        if state == UpdaterStatus.ERROR:
            print(f"Error: {error.name}")
            return

    start_time = time.perf_counter()

    data = bytearray(256+4)
    for i in range(block_count):
        block_idx = i
        data[:256] = firmware_data[block_idx*256:(block_idx+1)*256]
        data[256] = block_idx & 0xFF
        data[257] = (block_idx >> 8) & 0xFF
        crc_calc = crc16.compute(data[:256])
        data[258] = crc_calc & 0xFF
        data[259] = (crc_calc >> 8) & 0xFF

        state, error = get_state(mcu)
        print(f"State: {state.name}, Sending block {block_idx}/{block_count}")
        while state == UpdaterStatus.BUSY:
            time.sleep(0.1)
            state, error = get_state(mcu)

        if state == UpdaterStatus.ERROR:
            print(f"Error: {error.name}")
            return
        
        mcu.write_block(REG_FW_DATA, data)

    end_time = time.perf_counter()

    time.sleep(0.1)  # wait for the last block to be processed

    state, error = get_state(mcu)
    print(f"State: {state.name}")
    if state == UpdaterStatus.ERROR:
        print(f"Error: {error.name}")
        return

    print(f"Written in {end_time - start_time:.2f}s ({len(firmware_data)/(end_time - start_time)/1024*8:.2f} Kbits/s)")
    print("Update complete. Reset MCU to start new firmware.")

if __name__ == "__main__":
    main()