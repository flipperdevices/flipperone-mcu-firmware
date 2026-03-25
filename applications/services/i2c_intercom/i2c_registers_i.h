#pragma once

#include "i2c_registers.h"

#ifdef __cplusplus
extern "C" {
#endif

// Start register reading
bool i2c_register_read_start(uint16_t address, uint8_t* value);

// End register reading, and commit any side effects (e.g. clear read-to-clear registers)
bool i2c_register_read_commit(uint16_t address);

// Write value to register, return true if successful (address exists and is writable)
// TODO: This function should notify the register owner that a write has been made (callbacks? events?).
bool i2c_register_write(uint16_t address, uint8_t value);

#ifdef __cplusplus
}
#endif
