#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    I2CRegFlagRead = 1 << 0,
    I2CRegFlagWrite = 1 << 1,
    I2CRegFlagReadToClear = 1 << 2,
} I2CRegFlag;

void i2c_registers_init(void);
void i2c_register_add(uint16_t address, uint16_t default_value, uint32_t flags);
bool i2c_register_read_start(uint16_t address, uint8_t* value);
bool i2c_register_read_commit(uint16_t address);
bool i2c_register_write(uint16_t address, uint8_t value);
void i2c_register_update(uint16_t address, uint16_t value, uint16_t mask);
void i2c_register_update_and_set_interrupt(uint16_t address, uint16_t value, uint16_t mask, uint16_t interrupt_address, uint16_t interrupt_bits);

#ifdef __cplusplus
}
#endif
