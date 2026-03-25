#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    I2CRegFlagRead = 1 << 0,
    I2CRegFlagWrite = 1 << 1,
    I2CRegFlagReadToClear = 1 << 2,
} I2CRegFlag;

// Init register storage
void i2c_registers_init(void);

// Add a register to the register map. Address must be even, value is 16-bit but accessed as big-endian 8-bit.
// Flags specify the register properties (readable, writable, read-to-clear), see I2CRegFlag.
void i2c_register_add(uint16_t address, uint16_t default_value, uint32_t flags);

// Add mapping from register address to status register bit. This is used to update the status register when the register value changes. Bit specify which bit in the status register to update when the register value changes.
void i2c_address_to_status_bits_map_add(uint8_t bit, uint16_t address);

// Update register value with a mask. This is used to update only some bits of the register without affecting other bits.
void i2c_register_update(uint16_t address, uint16_t value, uint16_t mask);

// Update register value with a mask, and set interrupt if needed. This is used to update only some bits of the register without affecting other bits.
// Interrupt register must exist, and interrupt_bits specify which bits to set in the interrupt register. Also will set the interrupt line.
void i2c_register_update_and_set_interrupt(uint16_t address, uint16_t value, uint16_t mask, uint16_t interrupt_address, uint16_t interrupt_bits);

#ifdef __cplusplus
}
#endif
