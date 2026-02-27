#pragma once

#include <furi_hal_i2c_types.h>
#include <furi_hal_gpio.h>
#include "bq25792_reg.h"

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
Bq25792Status bq25792_get_ibus_ma(Bq25792* instance, int16_t* ibus);
Bq25792Status bq25792_get_ibat_ma(Bq25792* instance, int16_t* ibat);
Bq25792Status bq25792_get_vbus_mv(Bq25792* instance, uint16_t* vbus);
Bq25792Status bq25792_get_vbat_mv(Bq25792* instance, uint16_t* vbat);
Bq25792Status bq25792_get_vsys_mv(Bq25792* instance, uint16_t* vsys);
Bq25792Status bq25792_get_bat_pct(Bq25792* instance, float* bat_pct);
Bq25792Status bq25792_get_charger_temperature(Bq25792* instance, float* temperature);
Bq25792Status bq25792_get_temperature_battery_celsius(Bq25792* instance, float* bat_temperature);
Bq25792Status bq25792_get_input_current_limit_ma(Bq25792* instance, uint16_t* input_current_limit);
Bq25792Status bq25792_set_input_current_limit_ma(Bq25792* instance, uint16_t input_current_limit);
Bq25792Status bq25792_get_charge_voltage_limit_ma(Bq25792* instance, uint16_t* charge_voltage_limit);
Bq25792Status bq25792_set_charge_voltage_limit_ma(Bq25792* instance, uint16_t charge_voltage_limit);
Bq25792Status bq25792_get_charge_current_limit_ma(Bq25792* instance, uint16_t* charge_current_limit);
Bq25792Status bq25792_set_charge_current_limit_ma(Bq25792* instance, uint16_t charge_current_limit);
Bq25792Status bq25792_charge_enable(Bq25792* instance, bool enable);
Bq25792Status bq25792_get_charger_status(Bq25792* instance, Bq25792ChargerStatusReg* status);
Bq25792Status bq25792_get_charger_fault(Bq25792* instance, Bq25792FaultStatusReg* fault);
Bq25792Status bq25792_get_charger_irq_flags(Bq25792* instance, Bq25792ChargerFlagReg* irq_flags);
#ifdef __cplusplus
}
#endif
