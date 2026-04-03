#include "test_peref.h"
#include <furi.h>

#include <furi_hal_resources.h>
#include <furi_hal_gpio.h>

#include <furi_hal_i2c.h>
#include <furi_hal_i2c_config.h>
#include <hardware/gpio.h>
#include <input/input.h>
#include <furi_hal_nvm.h>
#include <power/power.h>

#include <drivers/ina4230/ina4230.h>

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

    furi_log_set_level(FuriLogLevelDebug);
    FURI_LOG_T("tag", "Trace");
    FURI_LOG_D("tag", "Debug");
    FURI_LOG_I("tag", "Info");
    FURI_LOG_W("tag", "Warning");
    FURI_LOG_E("tag", "Error");

    uint8_t duty = 0;
    UNUSED(duty);

    //test_nvm();

    //Power* power = furi_record_open(RECORD_POWER);

    Ina4230* ina4230_add[5] = {0};

    ina4230_add[0] = ina4230_init(&furi_hal_i2c_handle_main, 0x40);
    ina4230_set_config_channel(ina4230_add[0], 0, "VDD_0V75_S3   ", 0.050f, 0.5f);
    ina4230_set_config_channel(ina4230_add[0], 1, "VCC3V3_CONTROL", 0.010f, 6.0f);
    ina4230_set_config_channel(ina4230_add[0], 2, "VDD0V85_DDR_S0", 0.020f, 3.0f);
    ina4230_set_config_channel(ina4230_add[0], 3, "VCC_3V3_S3    ", 0.010f, 5.0f);

    ina4230_add[1] = ina4230_init(&furi_hal_i2c_handle_main, 0x41);
    ina4230_set_config_channel(ina4230_add[1], 0, "VDDQ0V51_DDR_S0", 0.020f, 3.0f);
    ina4230_set_config_channel(ina4230_add[1], 1, "VDD0V75_NPU_S0", 0.010f, 5.0f);
    ina4230_set_config_channel(ina4230_add[1], 2, "VDD0V75_GPU_S0", 0.020f, 3.0f);
    ina4230_set_config_channel(ina4230_add[1], 3, "VDD0V75_LOGIC_S0", 0.020f, 3.0f);

    ina4230_add[2] = ina4230_init(&furi_hal_i2c_handle_main, 0x46);
    ina4230_set_config_channel(ina4230_add[2], 0, "VCCA_3V3_S0   ", 0.020f, 0.5f);
    ina4230_set_config_channel(ina4230_add[2], 1, "VCCIO3V3/1V8_SD_S0", 0.050f, 0.3f);
    ina4230_set_config_channel(ina4230_add[2], 2, "VDD2_1V05_DDR_S3", 0.020f, 2.5f);
    ina4230_set_config_channel(ina4230_add[2], 3, "VCC_1V8_S3    ", 0.020f, 3.0f);

    ina4230_add[3] = ina4230_init(&furi_hal_i2c_handle_main, 0x43);
    ina4230_set_config_channel(ina4230_add[3], 0, "VDD0V75_CPU_BIG_S0", 0.010f, 6.5f);
    ina4230_set_config_channel(ina4230_add[3], 1, "VDDA_1V2_S0   ", 0.050f, 0.3f);
    ina4230_set_config_channel(ina4230_add[3], 2, "VCCA_1V8_S0   ", 0.020f, 0.5f);
    ina4230_set_config_channel(ina4230_add[3], 3, "VDD0V75_CPU_LIT_S0", 0.010f, 5.0f);

    ina4230_add[4] = ina4230_init(&furi_hal_i2c_handle_main, 0x44);
    ina4230_set_config_channel(ina4230_add[4], 0, "VDDA_0V75_S0  ", 0.050f, 0.3f);
    ina4230_set_config_channel(ina4230_add[4], 1, "VDDA_0V85_S0  ", 0.020f, 0.5f);
    ina4230_set_config_channel(ina4230_add[4], 2, "VDDA0V75_HDMI_S0", 0.020f, 0.5f);
    ina4230_set_config_channel(ina4230_add[4], 3, "VDDA0V85_DDR_PLL_S0", 0.050f, 0.3f);

    while(true) {
        FURI_LOG_I(TAG, "Test");
        furi_delay_ms(500);
        for(uint32_t ina = 0; ina < 5; ina++) {
            FURI_LOG_I(TAG, "INA4230 at address %ld", ina);
            for(uint32_t channel = 0; channel < 4; channel++) {
                float bus_voltage = ina4230_get_bus_voltage_v(ina4230_add[ina], channel);
                float shunt_voltage = ina4230_get_shunt_voltage_mv(ina4230_add[ina], channel);
                float current = ina4230_get_current_a(ina4230_add[ina], channel);
                float power = ina4230_get_power_w(ina4230_add[ina], channel);
                const char* name = ina4230_get_channel_name(ina4230_add[ina], channel);
                FURI_LOG_I(
                    TAG,
                    "Channel %ld (%s): \tBus Voltage: %.3f V, \tShunt Voltage: %.3f mV, \tCurrent: %.3f A, \tPower: %.3f W",
                    channel,
                    name,
                    bus_voltage,
                    shunt_voltage,
                    current,
                    power);
            }
        }
    }
    furi_crash();
}
