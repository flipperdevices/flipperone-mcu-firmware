#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <furi_hal_i2c_types.h>

#define TPS62868_ADDRESS 0x47
typedef struct Tps62868x Tps62868x;

#ifdef __cplusplus
extern "C" {
#endif

Tps62868x* tps62868x_init(const FuriHalI2cBusHandle* handle, uint8_t address);
void tps62868x_deinit(Tps62868x* instance);
void tps62868x_set_pwm_on(Tps62868x* instance);
void tps62868x_set_pwm_off(Tps62868x* instance);
int tps62868x_read_reg(Tps62868x* instance, uint8_t reg, uint8_t* data);
int tps62868x_write_reg(Tps62868x* instance, uint8_t reg, uint8_t* data);
int tps62868x_set_voltage(Tps62868x* instance, float volt);
float tps62868x_get_voltage(Tps62868x* instance);	

#ifdef __cplusplus
}
#endif
