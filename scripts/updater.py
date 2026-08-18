from periphery import I2C

MCU_ADDR = 0x69

def read_i2c_register(i2c, register_address):
    # Split the register address into two bytes (high and low)
    high_byte = (register_address >> 8) & 0xFF
    low_byte = register_address & 0xFF

    # Create a message to write the register address
    write_msg = I2C.Message([high_byte, low_byte])

    # Create a message to read the data (2 bytes)
    read_msg = I2C.Message([0x00, 0x00], read=True)

    # Perform the transfer
    i2c.transfer(MCU_ADDR, [write_msg, read_msg])

    read_word = (read_msg.data[1] << 8) | read_msg.data[0]

    # Return the read data
    return read_word

def write_i2c_register(i2c, register_address, value):
    # Split the register address into two bytes (high and low)
    high_byte = (register_address >> 8) & 0xFF
    low_byte = register_address & 0xFF

    # Split the value into two bytes (high and low)
    value_high_byte = (value >> 8) & 0xFF
    value_low_byte = value & 0xFF

    # Create a message to write the register address and value
    write_msg = I2C.Message([high_byte, low_byte, value_high_byte, value_low_byte])

    # Perform the transfer
    i2c.transfer(MCU_ADDR, [write_msg])

def write_i2c_buf(i2c, register_address, buf):
    # Split the register address into two bytes (high and low)
    high_byte = (register_address >> 8) & 0xFF
    low_byte = register_address & 0xFF

    # Create a message to write the register address and buffer
    write_msg = I2C.Message([high_byte, low_byte] + list(buf))

    # Perform the transfer
    i2c.transfer(MCU_ADDR, [write_msg])

def main():
    # Open the I2C bus
    i2c = I2C("/dev/i2c-0")

    # Read from the register at address 0x0100
    register_address = 0x0200
    read_word = read_i2c_register(i2c, register_address)
    print(f"Read data from register 0x{register_address:04X}: 0x{read_word:04X}")

    write_i2c_buf(i2c, 0xF100, bytes(256))

    # Close the I2C bus
    i2c.close()

if __name__ == "__main__":
    main()