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

// Adds an interrupt register with a corresponding mask register and connects it to the status register.
void i2c_register_add_interrupt(uint16_t address, uint16_t mask_address, uint8_t status_register_bit);

// Update register value with a mask. This is used to update only some bits of the register without affecting other bits.
// @warning Must be called in a critical section, use with_i2c_register macro for convenience.
void i2c_register_update(uint16_t address, uint16_t value, uint16_t mask);

// Sets the interrupt bits if the interrupt is not masked. Also, the interrupt line will be raised if some interrupt bits are set.
// @warning Must be called in a critical section, use with_i2c_register macro for convenience.
void i2c_register_set_interrupt(uint16_t interrupt_address, uint16_t interrupt_bits);

#define with_i2c_register(code) \
    {                           \
        FURI_CRITICAL_ENTER();  \
        {code};                 \
        FURI_CRITICAL_EXIT();   \
    }

#ifdef __cplusplus
}
#endif
