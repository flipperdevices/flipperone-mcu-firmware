#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <furi_hal_i2c_types.h>

#ifdef __cplusplus
extern "C" {
#endif

bool tps62868x_init(FuriHalI2cBusHandle* handle);
void tps62868x_deinit(void);
void tps62868x_set_pwm_on(FuriHalI2cBusHandle* handle);
void tps62868x_set_pwm_off(FuriHalI2cBusHandle* handle);
int tps62868x_read_reg(FuriHalI2cBusHandle* handle, uint8_t reg, uint8_t* data);
int tps62868x_write_reg(FuriHalI2cBusHandle* handle, uint8_t reg, uint8_t* data);
int tps62868x_set_voltage(FuriHalI2cBusHandle* handle, float volt);
float tps62868x_get_voltage(FuriHalI2cBusHandle* handle);	

#ifdef __cplusplus
}
#endif
