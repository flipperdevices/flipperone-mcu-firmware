#pragma once

#include <furi_hal_i2c_types.h>
#include <furi_hal_gpio.h>

#define BQ25792_ADDRESS 0x6B

typedef struct Bq25792 Bq25792;
typedef void (*Bq25792CallbackInput)(void* context);

typedef enum {
    Bq25792StatusOk = 0,
    Bq25792StatusError = -1,
    Bq25792StatusTimeout = -2,
    Bq25792StatusUnknown = -3,
} Bq25792Status;

typedef enum {
    Bq25792PowerIdle = 0b00, /** Normal operation (default) */
    Bq25792PowerShutdown = 0b01, /** Shutdown mode*/
    Bq25792PowerShipMode = 0b10, /** Ship mode*/
    Bq25792PowerReset = 0b11, /** System power reset*/
} Bq25792PowerSwitch;

#ifdef __cplusplus
extern "C" {
#endif

Bq25792* bq25792_init(const FuriHalI2cBusHandle* i2c_handle, uint8_t address, const GpioPin* pin_interrupt);
void bq25792_deinit(Bq25792* instance);
Bq25792Status bq25792_set_power_switch(Bq25792* instance, Bq25792PowerSwitch power_switch);

#ifdef __cplusplus
}
#endif
