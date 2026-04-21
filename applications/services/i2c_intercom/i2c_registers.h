#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*I2CRegisterCallback)(void* context, uint16_t value);

// Init register storage
void i2c_registers_init(void);

// Add a readable register to the register map. Address must be even.
void i2c_register_add_readable(uint16_t address, uint16_t default_value);

// Add a writable register to the register map. Address must be even. Register will be readable as well.
void i2c_register_add_writable(uint16_t address, uint16_t default_value, I2CRegisterCallback write_callback, void* write_callback_context);

// Adds an interrupt register with a corresponding mask register and connects it to the status register.
void i2c_register_add_interrupt(uint16_t address, uint16_t mask_address, uint8_t status_register_bit);

// Update register value with a mask. This is used to update only some bits of the register without affecting other bits.
// @warning Must be called in a critical section, use with_i2c_register macro for convenience.
void i2c_register_update(uint16_t address, uint16_t value, uint16_t mask);

// Sets the interrupt bits if the interrupt is not masked. Also, the interrupt line will be raised if some interrupt bits are set.
// @warning Must be called in a critical section, use with_i2c_register macro for convenience.
void i2c_register_set_interrupt(uint16_t interrupt_address, uint16_t interrupt_bits);

// Macro for executing code in a critical section when accessing registers.
#define with_i2c_register(code) \
    {                           \
        FURI_CRITICAL_ENTER();  \
        {code};                 \
        FURI_CRITICAL_EXIT();   \
    }

#ifdef __cplusplus
}
#endif
