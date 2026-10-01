#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Callback type for writable registers. Address will be always even, value is what in the register after high byte is written.
typedef void (*I2CRegisterCallback)(void* context, uint16_t address, uint16_t value);

// Add a readable register to the register map. Address must be even.
void i2c_register_add_readable(uint16_t address, uint16_t default_value);

// Add a writable register to the register map. Address must be even. Register will be readable as well.
void i2c_register_add_writable(uint16_t address, uint16_t default_value, I2CRegisterCallback write_callback, void* write_callback_context);

// Adds an interrupt register with a corresponding mask register and connects it to the status register.
void i2c_register_add_interrupt(uint16_t address, uint16_t mask_address, uint8_t status_register_bit);

// Maximum number of byte-addressed regions, see i2c_register_add_region.
#define I2C_REGISTERS_MAX_REGIONS (4)

// Callbacks serving a byte-addressed region. Address is passed as `offset`
// relative to the region base, with no hi/lo byte splitting: a region is a
// plain byte window, not a sequence of 16-bit registers.
// @warning Called from the I2C slave ISR inside a critical section — must be
// short, non-blocking and must not allocate.
// @returns true if the access was served; false makes the transfer report an
// invalid address (reads yield 0x00), same as for an unmapped register.
typedef bool (*I2CRegionReadFn)(void* context, uint16_t offset, uint8_t* value);
typedef bool (*I2CRegionWriteFn)(void* context, uint16_t offset, uint8_t value);

// Map [base, base + length) onto storage owned by someone else. Unlike the
// registers added with i2c_register_add_*, a region keeps no data here and
// carries no 16-bit semantics — every byte goes straight to the callbacks.
// Base must be even; either callback may be NULL (making the region
// write-only or read-only respectively). Regions are matched before regular
// registers, so a region may not overlap another region or an existing
// register — both are checked here.
void i2c_register_add_region(uint16_t base, uint16_t length, I2CRegionReadFn read, I2CRegionWriteFn write, void* context);

// Update register value with a mask. This is used to update only some bits of the register without affecting other bits.
// @warning Must be called in a critical section, use with_i2c_register macro for convenience.
// @returns true if the register value is changed after the update, false otherwise.
bool i2c_register_update(uint16_t address, uint16_t value, uint16_t mask);

// Get register value. This is used to read the whole 16-bit value of the register, even if it is read/write or write-only.
// @warning Must be called in a critical section, use with_i2c_register macro for convenience.
// @returns value of the register.
uint16_t i2c_register_get_value(uint16_t address);

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
