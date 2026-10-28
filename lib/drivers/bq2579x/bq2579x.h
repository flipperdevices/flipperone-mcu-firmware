#pragma once

#include <furi_hal_i2c_types.h>
#include <furi_hal_gpio.h>
#include "bq2579x_reg.h"
#include "bq2579x_helper.h"

#define BQ2579X_ADDRESS 0x6B

typedef struct Bq2579x Bq2579x;
typedef void (*Bq2579xCallbackInput)(void* context);

typedef enum {
    Bq2579xStatusUnknown = 0,
    Bq2579xStatusOk = 1,
    Bq2579xStatusError = -1,
    Bq2579xStatusTimeout = -2,
} Bq2579xStatus;

typedef enum {
    Bq2579xChipBq25792, /** BQ25792, no MPPT/backup mode */
    Bq2579xChipBq25798, /** BQ25798 */
} Bq2579xChip;

typedef enum {
    Bq2579xWatchdogTimeDisabled = 0x00, /** disabled */
    Bq2579xWatchdogTime0_5s = 0x01, /** 0.5s */
    Bq2579xWatchdogTime1s = 0x02, /** 1s */
    Bq2579xWatchdogTime2s = 0x03, /** 2s */
    Bq2579xWatchdogTime20s = 0x04, /** 20s */
    Bq2579xWatchdogTime40s = 0x05, /** 40s (default) */
    Bq2579xWatchdogTime80s = 0x06, /** 80s */
    Bq2579xWatchdogTime160s = 0x07, /** 160s */
} Bq2579xWatchdogTime;

#ifdef __cplusplus
extern "C" {
#endif

Bq2579x* bq2579x_init(const FuriHalI2cBusHandle* i2c_handle, uint8_t address, const GpioPin* pin_interrupt);
void bq2579x_deinit(Bq2579x* instance);
Bq2579xChip bq2579x_get_chip(Bq2579x* instance);
Bq2579xStatus bq2579x_load_default_config(Bq2579x* instance);
Bq2579xStatus bq2579x_set_power_switch(Bq2579x* instance, Bq2579xPowerSwitch power_switch);
Bq2579xStatus bq2579x_get_ibus_ma(Bq2579x* instance, int16_t* ibus);
Bq2579xStatus bq2579x_get_ibat_ma(Bq2579x* instance, int16_t* ibat);
Bq2579xStatus bq2579x_get_vbus_mv(Bq2579x* instance, uint16_t* vbus);
Bq2579xStatus bq2579x_get_vbat_mv(Bq2579x* instance, uint16_t* vbat);
Bq2579xStatus bq2579x_get_vsys_mv(Bq2579x* instance, uint16_t* vsys);
Bq2579xStatus bq2579x_get_bat_pct(Bq2579x* instance, float* bat_pct);
Bq2579xStatus bq2579x_get_charger_temperature(Bq2579x* instance, float* temperature);
Bq2579xStatus bq2579x_get_temperature_battery_celsius(Bq2579x* instance, float* bat_temperature);
Bq2579xStatus bq2579x_get_input_current_limit_ma(Bq2579x* instance, uint16_t* input_current_limit);
Bq2579xStatus bq2579x_set_input_current_limit_ma(Bq2579x* instance, uint16_t input_current_limit);
Bq2579xStatus bq2579x_get_charge_voltage_limit_ma(Bq2579x* instance, uint16_t* charge_voltage_limit);
Bq2579xStatus bq2579x_set_charge_voltage_limit_ma(Bq2579x* instance, uint16_t charge_voltage_limit);
Bq2579xStatus bq2579x_get_charge_current_limit_ma(Bq2579x* instance, uint16_t* charge_current_limit);
Bq2579xStatus bq2579x_set_charge_current_limit_ma(Bq2579x* instance, uint16_t charge_current_limit);
Bq2579xStatus bq2579x_get_ico_current_limit_ma(Bq2579x* instance, uint16_t* ico_current_limit);
Bq2579xStatus bq2579x_charge_enable(Bq2579x* instance, bool enable);
Bq2579xStatus bq2579x_charge_is_enabled(Bq2579x* instance, bool* enabled);
Bq2579xStatus bq2579x_get_charger_status(Bq2579x* instance, Bq2579xChargerStatusReg* status);
Bq2579xStatus bq2579x_get_charger_fault(Bq2579x* instance, Bq2579xFaultStatusReg* fault);
Bq2579xStatus bq2579x_clear_charger_fault(Bq2579x* instance, Bq2579xFaultStatusReg* fault);
Bq2579xStatus bq2579x_get_charger_irq_flags(Bq2579x* instance, Bq2579xChargerFlagReg* irq_flags);
Bq2579xStatus bq2579x_adc_enable(Bq2579x* instance, bool enable);
Bq2579xStatus bq2579x_mppt_enable(Bq2579x* instance, bool enable);
Bq2579xStatus bq2579x_otg_enable(Bq2579x* instance, bool enable);

/** VINDPM: the input voltage the charger regulates down to before it starts
 * cutting input current. Hardware resets it to 3600 mV whenever the adapter
 * is unplugged, so it cannot be configured once at boot. */
Bq2579xStatus bq2579x_get_input_voltage_limit_mv(Bq2579x* instance, uint16_t* input_voltage_limit);

/** Sets VINDPM. Must be re-applied per PD contract, not once at boot: the
 * hardware picks its own value from the VBUS it measures at plug-in, which is
 * vSafe5V, and never revisits it when we negotiate a higher voltage. Left
 * alone, a 15 V contract keeps a ~4.4 V threshold and the charger will hold
 * full input current while the source collapses instead of backing off.
 * Clamped to the register's 3.6-22 V range. */
Bq2579xStatus bq2579x_set_input_voltage_limit_mv(Bq2579x* instance, uint16_t input_voltage_limit);

/** Input Current Optimization. When on, the charger probes above the
 * configured input current limit until the source droops, to discover what it
 * can really deliver — useful only for a source that does not advertise its
 * capability. Off after bq2579x_load_default_config(): overshooting a limit
 * we already know from PD or Type-C browns the source out. */
Bq2579xStatus bq2579x_ico_enable(Bq2579x* instance, bool enable);

/** REG16.VBUS_PD_EN: the internal 6 kΩ pull-down on VBUS. Off by default and
 * after REG_RST. Meant for bleeding VBUS down to vSafe0V after OTG is turned
 * off (PD PR_Swap source→sink); at 5 V it sinks ~0.8 mA, so it is slow for
 * large bulk capacitance and must be switched off again once VBUS is low. */
Bq2579xStatus bq2579x_vbus_pulldown_enable(Bq2579x* instance, bool enable);
Bq2579xStatus bq2579x_get_otg_voltage_mv(Bq2579x* instance, uint16_t* otg_voltage);
Bq2579xStatus bq2579x_set_otg_voltage_mv(Bq2579x* instance, uint16_t otg_voltage);
Bq2579xStatus bq2579x_get_otg_current_ma(Bq2579x* instance, uint16_t* otg_current);
Bq2579xStatus bq2579x_set_otg_current_ma(Bq2579x* instance, uint16_t otg_current);
Bq2579xStatus bq2579x_watchdog_reset(Bq2579x* instance);
Bq2579xStatus bq2579x_watchdog_set_time(Bq2579x* instance, Bq2579xWatchdogTime time);
#ifdef __cplusplus
}
#endif
