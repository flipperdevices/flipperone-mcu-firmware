#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <drivers/i2c_slave/i2c_slave.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback for writable registers.
 *
 * Runs in the register software interrupt, which applies host writes one after
 * another: keep it short and do not log, every write queued behind it waits.
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
 * Longest host write transaction, data bytes after the address. A longer one
 * is dropped whole and counted. Covers an updater frame (260 bytes) and a
 * UCSI MESSAGE_OUT (256 bytes).
 */
#define I2C_REGISTERS_TRANSACTION_MAX (512)

/**
 * @brief Write callback for a byte-addressed region.
 *
 * Gets the part of one host write transaction that falls inside the region,
 * in one call, however many bytes it carries.
 *
 * @param context Region context passed at registration.
 * @param offset Offset of data[0] relative to the region base.
 * @param data Bytes written.
 * @param length Number of bytes.
 * @return true if the write was accepted; false counts it as invalid.
 *
 * Called from interrupt context, like I2CRegisterCallback.
 */
typedef bool (*I2CRegionWriteFn)(void* context, uint16_t offset, const uint8_t* data, size_t length);

/**
 * @brief Map [base, base + length) onto storage owned by someone else.
 *
 * A region carries no 16-bit semantics: offsets are bytes relative to base.
 * Regions are matched before regular registers, so a region may not overlap
 * another region or an existing register; both are checked here.
 *
 * Reads are served by the I2C ISR straight from read_buffer, so it must be in
 * RAM (checked here). The owner updates it under with_i2c_register so that
 * the host never reads a half-updated buffer.
 *
 * @param base Region start address, must be even.
 * @param length Region size in bytes.
 * @param read_buffer Buffer of `length` bytes the host reads, NULL if the region is write-only.
 * @param write Write callback, NULL if the region is read-only.
 * @param context Passed to the write callback.
 */
void i2c_register_add_region(uint16_t base, uint16_t length, const volatile uint8_t* read_buffer, I2CRegionWriteFn write, void* context);

/**
 * @brief Update only the masked bits of a register.
 *
 * @param address Register address.
 * @param value New value.
 * @param mask Bits to update.
 * @return true if the register value changed, false otherwise.
 *
 * @warning Must be called inside with_i2c_register: FURI_CRITICAL does not mask the I2C ISR.
 */
bool i2c_register_update(uint16_t address, uint16_t value, uint16_t mask);

/**
 * @brief Get the whole 16-bit register value, including write-only registers.
 *
 * @param address Register address.
 * @return Register value.
 *
 * @warning Must be called inside with_i2c_register: FURI_CRITICAL does not mask the I2C ISR.
 */
uint16_t i2c_register_get_value(uint16_t address);

/**
 * @brief Set interrupt bits unless masked, raising the interrupt line when any are set.
 *
 * @param interrupt_address Interrupt register address.
 * @param interrupt_bits Bits to set.
 *
 * @warning Must be called inside with_i2c_register: FURI_CRITICAL does not mask the I2C ISR.
 */
void i2c_register_set_interrupt(uint16_t interrupt_address, uint16_t interrupt_bits);

/**
 * @brief Mask the I2C slave ISR, which FURI_CRITICAL does not mask.
 *
 * Raises BASEPRI to I2C_SLAVE_IRQ_PRIORITY. Keep the section short: it also
 * masks the scheduler and every interrupt below it.
 */
static inline __attribute__((always_inline)) uint32_t i2c_registers_lock(void) {
    uint32_t previous;
    __asm volatile("mrs %0, basepri" : "=r"(previous));
    __asm volatile("msr basepri_max, %0\n isb" : : "r"(I2C_SLAVE_IRQ_PRIORITY_RAW) : "memory");
    return previous;
}

static inline __attribute__((always_inline)) void i2c_registers_unlock(uint32_t previous) {
    __asm volatile("msr basepri, %0\n isb" : : "r"(previous) : "memory");
}

/** Execute `code` with the I2C slave ISR masked, for register access. */
#define with_i2c_register(code)                              \
    {                                                        \
        uint32_t __i2c_register_lock = i2c_registers_lock(); \
        {code};                                              \
        i2c_registers_unlock(__i2c_register_lock);           \
    }

#ifdef __cplusplus
}
#endif
