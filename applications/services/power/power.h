#pragma once
#include <furi.h>
#include <drivers/bq25792/bq25792_reg.h>
#include <drivers/bq25792/bq25792_helper.h>

#define RECORD_POWER "power"
typedef struct Power Power;

#ifdef __cplusplus
extern "C" {
#endif
FuriPubSub* power_get_pubsub(Power* power);

float_t power_ina219_get_voltage_v(Power* instance);
float_t power_ina219_get_current_a(Power* instance);
float_t power_ina219_get_power_w(Power* instance);
float_t power_ina219_get_shunt_voltage_mv(Power* instance);

bool power_bq25792_set_power_switch(Power* instance, Bq25792PowerSwitch power_switch);
bool power_bq25792_get_ibus_ma(Power* instance, int16_t* ibus);
bool power_bq25792_get_ibat_ma(Power* instance, int16_t* ibat);
bool power_bq25792_get_vbus_mv(Power* instance, uint16_t* vbus);
bool power_bq25792_get_vbat_mv(Power* instance, uint16_t* vbat);
bool power_bq25792_get_vsys_mv(Power* instance, uint16_t* vsys);
bool power_bq25792_get_charger_temperature(Power* instance, float* temperature);
bool power_bq25792_get_temperature_battery_celsius(Power* instance, float* temperature);
bool power_bq25792_get_input_current_limit_ma(Power* instance, uint16_t* input_current_limit);
bool power_bq25792_set_input_current_limit_ma(Power* instance, uint16_t input_current_limit);
bool power_bq25792_get_charge_voltage_limit_ma(Power* instance, uint16_t* charge_voltage_limit);
bool power_bq25792_set_charge_voltage_limit_ma(Power* instance, uint16_t charge_voltage_limit);
bool power_bq25792_get_charge_current_limit_ma(Power* instance, uint16_t* charge_current_limit);
bool power_bq25792_set_charge_current_limit_ma(Power* instance, uint16_t charge_current_limit);
bool power_bq25792_charge_enable(Power* instance, bool enable);
bool power_bq25792_get_charger_status(Power* instance, Bq25792ChargerStatusReg* status);
bool power_bq25792_get_charger_fault(Power* instance, Bq25792FaultStatusReg* fault);
bool power_bq25792_get_charger_irq_flags(Power* instance, Bq25792ChargerFlagReg* irq_flags);
bool power_bq25792_adc_enable(Power* instance, bool enable);
bool power_bq25792_watchdog_reset(Power* instance);
#ifdef __cplusplus
}
#endif
