#pragma once
#include <furi.h>
#include <drivers/bq2579x/bq2579x_reg.h>
#include <drivers/bq2579x/bq2579x_helper.h>
#include <drivers/bq28z620/bq28z620_reg.h>
#include <toolbox/furi_callback.h>

#define RECORD_POWER "power"

typedef struct Power Power;

typedef enum {
    PowerDeviceIna219 = (1 << 0),
    PowerDeviceBq2579x = (1 << 1),
    PowerDeviceBq28z620 = (1 << 2),
    PowerDeviceAllInit = (PowerDeviceIna219 | PowerDeviceBq2579x | PowerDeviceBq28z620),
} PowerDevice;

#ifdef __cplusplus
extern "C" {
#endif
FuriPubSub* power_get_pubsub(Power* power);

bool power_is_device_initialized(Power* instance, PowerDevice* device);

float_t power_ina219_get_voltage_v(Power* instance);
float_t power_ina219_get_current_a(Power* instance);
float_t power_ina219_get_power_w(Power* instance);
float_t power_ina219_get_shunt_voltage_mv(Power* instance);

bool power_bq2579x_reset_config(Power* instance);
bool power_bq2579x_set_power_switch(Power* instance, Bq2579xPowerSwitch power_switch);
bool power_bq2579x_get_ibus_ma(Power* instance, int16_t* ibus);
bool power_bq2579x_get_ibat_ma(Power* instance, int16_t* ibat);
bool power_bq2579x_get_vbus_mv(Power* instance, uint16_t* vbus);
bool power_bq2579x_get_vbat_mv(Power* instance, uint16_t* vbat);
bool power_bq2579x_get_vsys_mv(Power* instance, uint16_t* vsys);
bool power_bq2579x_get_charger_temperature(Power* instance, float* temperature);
bool power_bq2579x_get_temperature_battery_celsius(Power* instance, float* temperature);
bool power_bq2579x_get_input_current_limit_ma(Power* instance, uint16_t* input_current_limit);
bool power_bq2579x_set_input_current_limit_ma(Power* instance, uint16_t input_current_limit);
bool power_bq2579x_get_charge_voltage_limit_ma(Power* instance, uint16_t* charge_voltage_limit);
bool power_bq2579x_set_charge_voltage_limit_ma(Power* instance, uint16_t charge_voltage_limit);
bool power_bq2579x_get_charge_current_limit_ma(Power* instance, uint16_t* charge_current_limit);
bool power_bq2579x_set_charge_current_limit_ma(Power* instance, uint16_t charge_current_limit);
bool power_bq2579x_charge_enable(Power* instance, bool enable);
bool power_bq2579x_charge_is_enabled(Power* instance, bool* enabled);
bool power_bq2579x_get_charger_status(Power* instance, Bq2579xChargerStatusReg* status);
bool power_bq2579x_get_charger_fault(Power* instance, Bq2579xFaultStatusReg* fault);
bool power_bq2579x_get_charger_irq_flags(Power* instance, Bq2579xChargerFlagReg* irq_flags);
bool power_bq2579x_adc_enable(Power* instance, bool enable);
bool power_bq2579x_watchdog_reset(Power* instance);
bool power_bq2579x_get_ico_current_limit_ma(Power* instance, uint16_t* ico_current_limit);
bool power_bq2579x_set_otg_params(Power* instance, uint16_t voltage_mv, uint16_t current_ma);
bool power_bq2579x_otg_enable(Power* instance, bool enable);
bool power_bq2579x_usb_is_connected(Power* instance, bool* usb_connected);

/**
 * One reading of the charge situation, the way the UI and the power meter
 * agree on it. The charger is the authority: it is charging while it pushes
 * current (the gauge may already say 100 % then), and the battery is full
 * when it has terminated the charge. The cable is the charger's VBUS.
 */
typedef enum {
    PowerChargeStateIdle, /**< no input, or input but the charger is neither charging nor done */
    PowerChargeStateCharging, /**< input present and the charger charging (trickle to top-off) */
    PowerChargeStateFull, /**< input present and the charger reports charge termination */
} PowerChargeState;

typedef struct {
    PowerChargeState state;
    bool input_present;
    bool charger_charging;
    bool charger_fault;
    uint8_t soc;
    uint16_t vbus_mv;
    int16_t ibus_ma;
    uint32_t input_mw; /**< VBUS x IBUS, 0 without input */
    int16_t battery_ma; /**< battery current by the charger's ADC, negative on discharge (what Linux shows) */
    float battery_v; /**< battery voltage by the charger's ADC */
    uint32_t discharge_mw; /**< what the battery gives, 0 while it takes */
} PowerChargeInfo;

/** The charger, the gauge and the INA219 sit on a bus the Linux host also
 *  drives while it is powered. Every read of ours can then collide with its
 *  own, and enough of them make it lose its readings, so periodic readers
 *  check this: screens the host covers anyway skip the read, the power meter
 *  thins its reads out to one every few seconds. */
bool power_host_shares_bus(Power* instance);

/** Whether a gauge current/voltage pair can be real; a collision on the
 *  shared bus can return garbage that reads as a successful transfer. */
bool power_battery_reading_plausible(int16_t battery_ma, float battery_v);

/** Read the charger and the gauge; false if either is unreachable or the
 *  sample is implausible. Callers do their other power reads only after this
 *  one answered. */
bool power_read_charge_info(Power* instance, PowerChargeInfo* info);

/** OTG overcurrent (IINDPM/IOTG) callback. Runs in the BQ2579X IRQ worker thread — keep it short. */
void power_bq2579x_set_otg_overcurrent_callback(Power* instance, FuriCallback callback, void* context);

bool power_bq28z620_get_control_status(Power* instance, Bq28z620StdCmdControlStatusRegBits* control_status);
bool power_bq28z620_get_time_to_empty(Power* instance, uint16_t* time_to_empty);
bool power_bq28z620_get_temperature(Power* instance, float* temperature);
bool power_bq28z620_get_voltage(Power* instance, float* voltage);
bool power_bq28z620_get_battery_status(Power* instance, Bq28z620StdCmdBatteryStatusRegBits* battery_status);
bool power_bq28z620_get_current(Power* instance, int16_t* current);
bool power_bq28z620_get_remaining_capacity(Power* instance, uint16_t* remaining_capacity);
bool power_bq28z620_get_full_charge_capacity(Power* instance, uint16_t* full_charge_capacity);
bool power_bq28z620_get_average_current(Power* instance, int16_t* average_current);
bool power_bq28z620_get_average_time_to_empty(Power* instance, uint16_t* average_time_to_empty);
bool power_bq28z620_get_average_time_to_full(Power* instance, uint16_t* average_time_to_full);
bool power_bq28z620_get_standby_current(Power* instance, int16_t* standby_current);
bool power_bq28z620_get_standby_time_to_empty(Power* instance, uint16_t* standby_time_to_empty);
bool power_bq28z620_get_max_load_current(Power* instance, int16_t* max_load_current);
bool power_bq28z620_get_max_load_time_to_empty(Power* instance, uint16_t* max_load_time_to_empty);
bool power_bq28z620_get_average_power(Power* instance, int16_t* average_power);
bool power_bq28z620_get_internal_temperature(Power* instance, float* internal_temperature);
bool power_bq28z620_get_cycle_count(Power* instance, uint16_t* cycle_count);
bool power_bq28z620_get_relative_state_of_charge(Power* instance, uint8_t* relative_state_of_charge);
bool power_bq28z620_get_state_of_health(Power* instance, uint8_t* state_of_health);
bool power_bq28z620_get_charging_voltage(Power* instance, float* charging_voltage);
bool power_bq28z620_get_charging_current(Power* instance, int16_t* charging_current);
bool power_bq28z620_get_design_capacity(Power* instance, uint16_t* design_capacity);

#ifdef __cplusplus
}
#endif
