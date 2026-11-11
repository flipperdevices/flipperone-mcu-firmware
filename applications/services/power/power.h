#pragma once
#include <furi.h>
#include <drivers/bq2579x/bq2579x_reg.h>
#include <drivers/bq2579x/bq2579x_helper.h>
#include <drivers/bq28z620/bq28z620_reg.h>
#include <toolbox/furi_callback.h>
#include <cpu_mode/cpu_mode.h>

#define RECORD_POWER "power"

typedef struct Power Power;

typedef enum {
    PowerDeviceIna219 = (1 << 0),
    PowerDeviceBq2579x = (1 << 1),
    PowerDeviceBq28z620 = (1 << 2),
    PowerDeviceAllInit = (PowerDeviceIna219 | PowerDeviceBq2579x | PowerDeviceBq28z620),
} PowerDevice;

/** MCU response to the host (Linux) going down */
typedef enum {
    PowerHostResetResponseDisabled, /* Ignore host shutdown (default) */
    PowerHostResetResponseResetMcu, /* Reset the MCU when the host goes down */
    PowerHostResetResponseResetMcuAndPower, /* Also power-cycle the host */
    PowerHostResetResponseResetMcuImmediately, /* Reset the MCU now (not armed) */
    PowerHostResetResponseCount,
} PowerHostResetResponse;

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

/** Arm the MCU reset response to the host going down (I2C-resettable, RAM-only). */
bool power_set_host_reset_response(Power* instance, PowerHostResetResponse response);
bool power_get_host_reset_response(Power* instance, PowerHostResetResponse* response);

/** Delay between the triggering host state and the reset, in 100 ms units. */
bool power_set_host_reset_delay(Power* instance, uint16_t reset_delay_100ms);

/** Flash storage offset of a freshly written firmware partition.
 *
 * When set, an armed ResetMcu host-reset response boots that partition with
 * a try-before-you-buy boot instead of doing a plain reset. Set to 0 to
 * clear. RAM-only, cleared by any reset.
 */
bool power_set_update_base(Power* instance, size_t update_base);

/** Notify the power service of a host CPU state change (drives the armed response). */
void power_on_host_state_changed(Power* instance, CpuState cpu_state);

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
