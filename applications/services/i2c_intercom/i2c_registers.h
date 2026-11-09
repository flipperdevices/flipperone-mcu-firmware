#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback for writable registers.
 *
 * @param context Callback context passed at registration.
 * @param address Register address, always even.
 * @param value Register value after the high byte has been written.
 */
typedef void (*I2CRegisterCallback)(void* context, uint16_t address, uint16_t value);

/**
 * @brief Add a readable register to the register map.
 *
 * @param address Register address, must be even.
 * @param default_value Initial register value.
 */
void i2c_register_add_readable(uint16_t address, uint16_t default_value);

/**
 * @brief Add a writable register to the register map. The register is readable as well.
 *
 * @param address Register address, must be even.
 * @param default_value Initial register value.
 * @param write_callback Called when the high byte is written.
 * @param write_callback_context Context passed to the callback.
 */
void i2c_register_add_writable(uint16_t address, uint16_t default_value, I2CRegisterCallback write_callback, void* write_callback_context);

/**
 * @brief Add an interrupt register with its mask register, connected to the status register.
 *
 * @param address Interrupt register address.
 * @param mask_address Mask register address.
 * @param status_register_bit Status register bit driven by this interrupt.
 */
void i2c_register_add_interrupt(uint16_t address, uint16_t mask_address, uint8_t status_register_bit);

/** Maximum number of byte-addressed regions, see i2c_register_add_region. */
#define I2C_REGISTERS_MAX_REGIONS (4)

/**
 * @brief Read callback for a byte-addressed region.
 *
 * @param context Region context passed at registration.
 * @param offset Byte offset relative to the region base.
 * @param value Pointer to store the byte read.
 * @return true if the access was served; false makes the transfer report an
 *         invalid address (reads yield 0x00), same as for an unmapped register.
 *
 * @warning Called from the I2C slave ISR inside a critical section — must be
 *          short, non-blocking and must not allocate.
 */
typedef bool (*I2CRegionReadFn)(void* context, uint16_t offset, uint8_t* value);

/**
 * @brief Write callback for a byte-addressed region.
 *
 * @param context Region context passed at registration.
 * @param offset Byte offset relative to the region base.
 * @param value Byte to write.
 * @return true if the access was served, false otherwise.
 *
 * @warning Same ISR constraints as I2CRegionReadFn.
 */
typedef bool (*I2CRegionWriteFn)(void* context, uint16_t offset, uint8_t value);

/**
 * @brief Map [base, base + length) onto storage owned by someone else.
 *
 * A region keeps no data here and carries no 16-bit semantics — every byte
 * goes straight to the callbacks as an offset relative to the region base,
 * with no hi/lo byte splitting. Base must be even; either callback may be
 * NULL (making the region write-only or read-only respectively). Regions are
 * matched before regular registers, so a region may not overlap another
 * region or an existing register — both are checked here.
 *
 * @param base Region start address, must be even.
 * @param length Region size in bytes.
 * @param read Read callback, may be NULL.
 * @param write Write callback, may be NULL.
 * @param context Passed to both callbacks.
 */
void i2c_register_add_region(uint16_t base, uint16_t length, I2CRegionReadFn read, I2CRegionWriteFn write, void* context);

/**
 * @brief Update only the masked bits of a register.
 *
 * @param address Register address.
 * @param value New value.
 * @param mask Bits to update.
 * @return true if the register value changed, false otherwise.
 *
 * @warning Must be called in a critical section, use with_i2c_register macro for convenience.
 */
bool i2c_register_update(uint16_t address, uint16_t value, uint16_t mask);

/**
 * @brief Get the whole 16-bit register value, including write-only registers.
 *
 * @param address Register address.
 * @return Register value.
 *
 * @warning Must be called in a critical section, use with_i2c_register macro for convenience.
 */
uint16_t i2c_register_get_value(uint16_t address);

/**
 * @brief Set interrupt bits unless masked, raising the interrupt line when any are set.
 *
 * @param interrupt_address Interrupt register address.
 * @param interrupt_bits Bits to set.
 *
 * @warning Must be called in a critical section, use with_i2c_register macro for convenience.
 */
void i2c_register_set_interrupt(uint16_t interrupt_address, uint16_t interrupt_bits);

/** Execute `code` in a critical section when accessing registers. */
#define with_i2c_register(code) \
    {                           \
        FURI_CRITICAL_ENTER();  \
        {code};                 \
        FURI_CRITICAL_EXIT();   \
    }

#ifdef __cplusplus
}
#endif
