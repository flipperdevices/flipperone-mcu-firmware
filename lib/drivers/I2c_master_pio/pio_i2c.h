#pragma once

#include "i2c.pio.h"
#include <furi_hal_gpio.h>

typedef struct I2cMasterPio I2cMasterPio;


I2cMasterPio* pio_i2c_init(const GpioPin* sda_pin, const GpioPin* scl_pin, uint32_t speed);
void pio_i2c_deinit(I2cMasterPio* instance);
void pio_i2c_gpio_init(I2cMasterPio* instance);
void pio_i2c_gpio_deinit(I2cMasterPio* instance);

// ----------------------------------------------------------------------------
// Low-level functions

void pio_i2c_start(I2cMasterPio* instance);
void pio_i2c_stop(I2cMasterPio* instance);
void pio_i2c_repstart(I2cMasterPio* instance);

bool pio_i2c_check_error(I2cMasterPio* instance);
void pio_i2c_resume_after_error(I2cMasterPio* instance);

// If I2C is ok, block and push data. Otherwise fall straight through.
void pio_i2c_put_or_err(I2cMasterPio* instance, uint16_t data);
uint8_t pio_i2c_get(I2cMasterPio* instance);
// ----------------------------------------------------------------------------
// Transaction-level functions

int pio_i2c_write_blocking(I2cMasterPio* instance, uint8_t addr, const uint8_t* txbuf, uint len, bool nostop, absolute_time_t until);
int pio_i2c_read_blocking(I2cMasterPio* instance, uint8_t addr, uint8_t *rxbuf, uint len, bool nostop, absolute_time_t until);