#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <furi_hal_i2c_types.h>

#ifdef __cplusplus
extern "C" {
#endif

// uint8_t tps62868x_init(void);
// void tps62868x_deinit(void);
// uint8_t tps62868x_read_reg(uint8_t reg, uint8_t* data);
// uint8_t tps62868x_write_reg(uint8_t reg, uint8_t* data);
// uint8_t tps62868x_set_voltage(float volt);
// float tps62868x_get_voltage(void);

bool tps62868x_init(FuriHalI2cHandle* handle);
void tps62868x_deinit(void);
void tps62868x_set_pwm_on(FuriHalI2cHandle* handle);
void tps62868x_set_pwm_off(FuriHalI2cHandle* handle);
int tps62868x_read_reg(FuriHalI2cHandle* handle, uint8_t reg, uint8_t* data);
int tps62868x_write_reg(FuriHalI2cHandle* handle, uint8_t reg, uint8_t* data);
int tps62868x_set_voltage(FuriHalI2cHandle* handle, float volt);
float tps62868x_get_voltage(FuriHalI2cHandle* handle);	

#ifdef __cplusplus
}
#endif
