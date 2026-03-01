#include "test_peref.h"
#include "core/log.h"
#include <furi.h>

#include <furi_hal_resources.h>
#include <furi_hal_gpio.h>

#include <furi_hal_i2c.h>
#include <furi_hal_i2c_config.h>
#include <hardware/gpio.h>
#include <input/input.h>
#include <furi_hal_nvm.h>
#include <power/power.h>

#define TAG "PerefTest"

void test_nvm(void) {
    FuriHalNvmStorage res;
    int32_t int_value = -123456;
    FuriString* str_value = furi_string_alloc();
    furi_string_set_str(str_value, "Hello, NVM!");

    // Test int32
    res = furi_hal_nvm_set_int32("int_key", int_value);
    FURI_LOG_I(TAG, "Set int32 result: %d", res);

    int32_t read_int_value = 0;
    res = furi_hal_nvm_get_int32("int_key", &read_int_value);
    FURI_LOG_I(TAG, "Get int32 result: %d, value_set: %ld value_get: %ld", res, int_value, read_int_value);

    // Test string
    res = furi_hal_nvm_set_str("str_key", str_value);
    FURI_LOG_I(TAG, "Set string result: %d", res);

    FuriString* read_str_value = furi_string_alloc();
    res = furi_hal_nvm_get_str("str_key", read_str_value);
    FURI_LOG_I(TAG, "Get string result: %d, value_set: %s value: %s", res, furi_string_get_cstr(str_value), furi_string_get_cstr(read_str_value));

    furi_string_free(str_value);
    furi_string_free(read_str_value);

    // Test delete
    res = furi_hal_nvm_get_int32("int_key", &read_int_value);
    FURI_LOG_I(TAG, "Delete int_key  result: %d, value_get: %ld", res, read_int_value);
    res = furi_hal_nvm_delete("int_key");
    FURI_LOG_I(TAG, "Delete int_key result: %d", res);

    // Try to get deleted key
    res = furi_hal_nvm_get_int32("int_key", &read_int_value);
    FURI_LOG_I(TAG, "Get deleted int_key result: %d", res);

    // Test UINT32
    uint32_t uint_value = 123456;
    res = furi_hal_nvm_set_uint32("uint_key", uint_value);
    FURI_LOG_I(TAG, "Set uint32 result: %d", res);
    uint32_t read_uint_value = 0;
    res = furi_hal_nvm_get_uint32("uint_key", &read_uint_value);
    FURI_LOG_I(TAG, "Get uint32 result: %d, value_set: %lu value_get: %lu", res, uint_value, read_uint_value);

    // test bool
    bool bool_value = true;
    res = furi_hal_nvm_set_bool("bool_key", bool_value);
    FURI_LOG_I(TAG, "Set bool result: %d", res);
    bool read_bool_value = false;
    res = furi_hal_nvm_get_bool("bool_key", &read_bool_value);
    FURI_LOG_I(TAG, "Get bool result: %d, value_set: %d value_get: %d", res, bool_value, read_bool_value);
}

int32_t test_peref_srv(void* p) {
    UNUSED(p);

    //furi_log_set_level(FuriLogLevelDebug);
    FURI_LOG_T("tag", "Trace");
    FURI_LOG_D("tag", "Debug");
    FURI_LOG_I("tag", "Info");
    FURI_LOG_W("tag", "Warning");
    FURI_LOG_E("tag", "Error");

    uint8_t duty = 0;
    UNUSED(duty);

    test_nvm();

    // Ina219* ina219 = ina219_init(&furi_hal_i2c_handle_external, INA219_ADDRESS, 0.004f, 9.0f); // 0.004 Ohm shunt, 0.4A max

    //display_h = furi_record_open(RECORD_DISPLAY);

    // display_h = display_jd9853_qspi_init();
    // display_jd9853_qspi_set_brightness(display_h, 20);
    Power* power = furi_record_open(RECORD_POWER);

    while(true) {
        furi_delay_ms(500);
        float bus_v = 0;
        float current_a = 0;
        float power_w = 0;
        float shunt_mv = 0;
        int16_t get_ibus_ma = 0;
        int16_t get_ibat_ma = 0;
        uint16_t get_vbus_mv = 0;
        uint16_t get_vbat_mv = 0;
        uint16_t get_vsys_mv = 0;
        float get_charger_temperature = 0;
        float get_temperature_battery_celsius = 0;
        uint16_t get_input_current_limit_ma = 0;
        uint16_t get_charge_voltage_limit_ma = 0;
        uint16_t get_charge_current_limit_ma = 0;
        Bq25792ChargerStatusReg get_charger_status = {0};
        Bq25792FaultStatusReg get_charger_fault = {0};
        Bq25792ChargerFlagReg get_charger_irq_flags = {0};

        bus_v = power_ina219_get_voltage_v(power);
        current_a = power_ina219_get_current_a(power);
        power_w = power_ina219_get_power_w(power);
        shunt_mv = power_ina219_get_shunt_voltage_mv(power);
        FURI_LOG_I(TAG, "Ina219\t VSYS: %.3f V | ISYS: %.2f mA | Shunt Voltage: %.4f mV |  Power: %.2f W", bus_v, current_a * 1000.0f, shunt_mv, power_w);

        power_bq25792_get_ibus_ma(power, &get_ibus_ma);
        power_bq25792_get_ibat_ma(power, &get_ibat_ma);
        power_bq25792_get_vbus_mv(power, &get_vbus_mv);
        power_bq25792_get_vbat_mv(power, &get_vbat_mv);
        power_bq25792_get_vsys_mv(power, &get_vsys_mv);
        power_bq25792_get_charger_temperature(power, &get_charger_temperature);
        power_bq25792_get_temperature_battery_celsius(power, &get_temperature_battery_celsius);
        power_bq25792_get_input_current_limit_ma(power, &get_input_current_limit_ma);
        power_bq25792_get_charge_voltage_limit_ma(power, &get_charge_voltage_limit_ma);
        power_bq25792_get_charge_current_limit_ma(power, &get_charge_current_limit_ma);
        power_bq25792_get_charger_status(power, &get_charger_status);
        power_bq25792_get_charger_fault(power, &get_charger_fault);
        power_bq25792_get_charger_irq_flags(power, &get_charger_irq_flags);

        FURI_LOG_I(
            TAG,
            "BQ25792\t VSYS: %.3f V | VBUS: %.3f V | IBUS: %d mA | VBAT: %.3f V | IBAT: %d mA \r\nCharger Temp: %.2f C | Battery Temp: %.2f C | Input Current Limit: %d mA | Charge Voltage Limit: %d mV | Charge Current Limit: %d mA\n",
            (float_t)get_vsys_mv / 1000.0f,
            (float_t)get_vbus_mv / 1000.0f,
            get_ibus_ma,
            (float_t)get_vbat_mv / 1000.0f,
            get_ibat_ma,
            get_charger_temperature,
            get_temperature_battery_celsius,
            get_input_current_limit_ma,
            get_charge_voltage_limit_ma,
            get_charge_current_limit_ma);

        FURI_LOG_I(
            TAG,
            "Charger Status0: %08b\r\n"
            "Charger Status1: %08b\r\n"
            "Charger Status2: %08b\r\n"
            "Charger Status3: %08b\r\n"
            "Charger Status4: %08b\r\n",
            get_charger_status.data[0],
            get_charger_status.data[1],
            get_charger_status.data[2],
            get_charger_status.data[3],
            get_charger_status.data[4]);
        FURI_LOG_I(
            TAG,
            "Charger Fault0: %08b\r\n"
            "Charger Fault1: %08b\r\n",
            get_charger_fault.data[0],
            get_charger_fault.data[1]);
        FURI_LOG_I(
            TAG,
            "Charger IRQ Flags0: %08b\r\n"
            "Charger IRQ Flags1: %08b\r\n"
            "Charger IRQ Flags2: %08b\r\n"
            "Charger IRQ Flags3: %08b\r\n",
            get_charger_irq_flags.data[0],
            get_charger_irq_flags.data[1],
            get_charger_irq_flags.data[2],
            get_charger_irq_flags.data[3]);
    }
    furi_crash();
}
