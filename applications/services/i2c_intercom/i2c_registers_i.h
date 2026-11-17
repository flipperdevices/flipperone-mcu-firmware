#pragma once

#include "i2c_registers.h"

#ifdef __cplusplus
extern "C" {
#endif

// Init register storage
void i2c_registers_init(void);

// Start register reading
// @warning Called from the I2C ISR only.
bool i2c_register_read_start(uint16_t address, uint8_t* value);

// End register reading, and commit any side effects (e.g. clear read-to-clear registers)
// @warning Called from the I2C ISR only.
bool i2c_register_read_commit(uint16_t address);

// Forget the high byte latched by the last low-byte read: call when a new
// transaction or address starts.
// @warning Called from the I2C ISR only.
void i2c_register_read_reset(void);

// Write transaction to the ring: begin at the address of the first data byte,
// append data bytes, commit once the transaction ended (STOP, repeated START,
// a new transaction). Nothing is applied before the commit; a transaction
// that does not fit in the ring is dropped whole on commit.
// @warning Called from the I2C ISR only.
void i2c_registers_write_begin(uint16_t address);
void i2c_registers_write_byte(uint8_t value);
void i2c_registers_write_commit(void);

// Monotonic counters for the intercom log.
uint32_t i2c_registers_dropped_transactions_get(void);
uint32_t i2c_registers_invalid_writes_get(void);

#ifdef __cplusplus
}
#endif
