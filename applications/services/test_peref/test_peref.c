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
#include <drivers/bq28z620/bq28z620.h>

#include <drivers/headphones/headphones.h>

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

bool state = false;
HeadphonesStatus _hp_status = 0;

void test_peref_srv_hp_callback(void* context, bool connected) {
    _hp_status = connected;
    state = 1;
}

void test_peref_srv_gpio_callback(void* context) {
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

    Power* power = furi_record_open(RECORD_POWER);
    headphones_init(&gpio_audio_hp_detect, &gpio_audio_key, test_peref_srv_hp_callback, NULL);

    //furi_hal_gpio_init_simple(&gpio_audio_hp_detect, GpioModeInput);
    // furi_hal_gpio_add_int_callback(&gpio_audio_hp_detect, GpioConditionRiseFall, test_peref_srv_gpio_callback, NULL);
    bool show_status = false;
    while(true) {
        //\FURI_LOG_I(TAG, "\r\n\r\nBattery status update");
        furi_delay_ms(100);

        if(state || show_status) {
            if(headphones_update(&_hp_status)) {
                FURI_LOG_I(TAG, "Headphones status changed: %08b", _hp_status);
            }
        }

        if(!state){
            show_status = false;
        } else {
            show_status = true;
        }
    }
    furi_crash();
}
