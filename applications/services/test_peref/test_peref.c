#include "test_peref.h"
#include <furi.h>

#include <furi_hal_resources.h>
#include <furi_hal_gpio.h>

#include <furi_hal_pwm.h>
//#include <drivers/ws2812/ws2812.h>

#include <furi_hal_i2c.h>
#include <drivers/tca6416a/tca6416a.h>
#include <furi_hal_power.h>
#include <drivers/drv2605l/drv2605l.h>
#include <furi_hal_i2c_config.h>
#include <drivers/iqs7211e/iqs7211e.h>
#include <drivers/ina219/ina219.h>
#include <drivers/bq25792/bq25792.h>
#include <drivers/tps62868x/tps62868x.h>
#include <drivers/fusb302/fusb302.h>
#include <display/display.h>
#include <hardware/gpio.h>
#include <input/input.h>
#include <drivers/display/display_jd9853_qspi.h>
#include <drivers/display/display_jd9853_reg.h>
#include <drivers/display/jd9853_reg.h>
#include <status_lights/status_lights.h>
#include <status_lights/status_lights_notification.h>
#include <tusb.h>
#include <furi_hal_nvm.h>

#define TAG "PerefTest"
DisplayJd9853QSPI* display_h = NULL;
int mod = 1;
bool eco = false;

// static void input_events_callback(const void* value, void* ctx) {
//     furi_assert(value);
//     furi_assert(ctx);

//     const InputEvent* event = value;
//     if(event->type == InputTypePress) {
//         if(event->key == InputKeyUp) {
//             display_jd9853_qspi_set_brightness(display_h, display_jd9853_qspi_get_brightness(display_h) + 2);
//         } else if(event->key == InputKeyRight) {
//             float vci = display_jd9853_qspi_get_vci(display_h);
//             vci += 0.1f * mod;
//             if(vci < 2.0f) vci = 2.0f;
//             if(vci > 3.3f) vci = 3.3f;
//             display_jd9853_qspi_set_vci(display_h, vci);
//             FURI_LOG_I(TAG, "Display VCI set to %.2f V", vci);

//         } else if(event->key == InputKeyDown) {
//             display_jd9853_qspi_set_brightness(display_h, display_jd9853_qspi_get_brightness(display_h) - 2);
//         } else if(event->key == InputKeyLeft) {
//             float vci = display_jd9853_qspi_get_vci(display_h);
//             vci -= 0.1f * mod;
//             if(vci < 2.0f) vci = 2.0f;
//             if(vci > 3.3f) vci = 3.3f;
//             display_jd9853_qspi_set_vci(display_h, vci);
//             FURI_LOG_I(TAG, "Display VCI set to %.2f V", vci);
//         } else if(event->key == InputKeyOk) {
//             eco = !eco;
//             display_jd9853_qspi_eco_mode(display_h, eco);
//         }

//         if(event->key == InputKey1) {
//             display_jd9853_load_config((DisplayJd9853QSPI*)display_h, jd9853_init_seq_2025_04_01_normal_white_no_reset);
//             FURI_LOG_I(TAG, "Display set to white mode");
//         } else if(event->key == InputKey5) {
//             display_jd9853_load_config((DisplayJd9853QSPI*)display_h, jd9853_init_seq_2025_04_01_normal_white_mod_no_reset);
//             FURI_LOG_I(TAG, "Display set to white mode");
//         } else if(event->key == InputKey2) {
//             if(mod == 1)
//                 mod = -1;
//             else
//                 mod = 1;
//         }
//     }
// }

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

    // GpioPin* ws2812_pins = (GpioPin*)malloc(sizeof(GpioPin) * 3);
    // ws2812_pins[0] = gpio_status_led_line1;
    // ws2812_pins[1] = gpio_status_led_line2;
    // ws2812_pins[2] = gpio_status_led_line3;
    // Ws2812* ws2812 = ws2812_init(ws2812_pins, 3);
    // free(ws2812_pins);

    // //furi_hal_i2c_bus_scan_print(&furi_hal_i2c_handle_internal);
    // //furi_delay_ms(1000);
    // uint8_t index_led[3] = {0};

   // Ina219* ina219 = ina219_init(&furi_hal_i2c_handle_external, INA219_ADDRESS, 0.004f, 9.0f); // 0.004 Ohm shunt, 0.4A max

    //display_h = furi_record_open(RECORD_DISPLAY);

    // display_h = display_jd9853_qspi_init();
    // display_jd9853_qspi_set_brightness(display_h, 20);

    // FuriPubSub* input = furi_record_open(RECORD_INPUT_EVENTS);
    // FuriPubSubSubscription* input_subscription = furi_pubsub_subscribe(input, input_events_callback, NULL);

    //StatusLights* status_lights = furi_record_open(RECORD_STATUS_LIGHTS);
    //Bq25792* bq25792 = bq25792_init(&furi_hal_i2c_handle_external, BQ25792_ADDRESS, NULL);

    // Fusb302* fusb302 = fusb302_init(&furi_hal_i2c_handle_external, FUSB302_ADDRESS, &gpio_mcu_gpio0);

    while(true) {
        furi_delay_ms(2000);

        status_lights_notification_send(notification_all_leds_on);
        furi_delay_ms(2000);

        status_lights_notification_send(notification_all_leds_white);


        // bq25792_set_power_switch(bq25792, Bq25792PowerShipMode);
        // FURI_LOG_I(TAG, "BQ25792 set to shutdown mode");
        //    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
        //     furi_hal_i2c_bus_scan_print(&furi_hal_i2c_handle_external);
        //    furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        // fusb302_read_cc_status(fusb302,1);
        //if(fusb302_read_role(fusb302)) {
        // FURI_LOG_I(TAG, "Role toggle completed!");
        //fusb302_pd_reset(fusb302);
        // } else {
        // FURI_LOG_I(TAG, "Role toggle not completed yet...");
        // }
        // const size_t width = JD9853_WIDTH; // 1 byte per pixel
        // const size_t height = JD9853_HEIGHT;
        // uint8_t ton[18] = {0xFC, 0x0, 0x11, 0x22, 0x32, 0x43, 0x54, 0x65, 0x76, 0x86, 0x97, 0xA8, 0xB9, 0xCA, 0xDA, 0xEB, 0xFC, 0x0};
        // uint8_t ton8[10] = {0xFC, 0x0, 36, 72, 108, 144, 180, 216, 252, 0x0};
        // uint8_t* data = (uint8_t*)malloc(width * height);
        // uint8_t val = 1;
        // for(size_t w = 0; w < width; w += 1) {
        //     for(size_t h = 0; h < height - 40; h += 1) {
        //         data[w + h * width] = ton[val];
        //     }
        //     if(w % 16 == 10) {
        //         val++;
        //     }
        // }
        // val = 16;
        // for(size_t w = 0; w < width; w += 1) {
        //     for(size_t h = 40; h < height - 70; h += 1) {
        //         data[w + h * width] = ton[val];
        //     }
        //     if(w % 16 == 10) {
        //         val--;
        //     }
        // }

        // val = 1;
        // for(size_t w = 0; w < width; w += 1) {
        //     for(size_t h = 70; h < height - 35; h += 1) {
        //         data[w + h * width] = ton8[val];
        //     }
        //     if(w % 32 == 26) {
        //         val += 1;
        //     }
        // }

        // val = 8;
        // for(size_t w = 0; w < width; w += 1) {
        //     for(size_t h = 70 + 35; h < height; h += 1) {
        //         data[w + h * width] = ton8[val];
        //     }
        //     if(w % 32 == 26) {
        //         val -= 1;
        //     }
        // }

        // for(size_t i = (width) * (height - 10); i < width * height; i += 1) {
        //     data[i] = 0x0;
        // }

        // for(size_t i = 0; i < (width * 10); i += 1) {
        //     data[i] = 0xff;
        // }
        // display_jd9853_qspi_write_buffer((DisplayJd9853QSPI*)display_h, data, width * height);
        // free(data);

        furi_delay_ms(1000);

        // float bus_v = ina219_get_bus_voltage_v(ina219);
        // float current_a = ina219_get_current_a(ina219);
        // float power_w = ina219_get_power_w(ina219);
        // float shunt_mv = ina219_get_shunt_voltage_mv(ina219);
        // FURI_LOG_I("Ina219", "Bus Voltage: %.3f V | Shunt Voltage: %.4f mV | Current: %.2f mA | Power: %.2f W",
        //     bus_v,
        //     shunt_mv,
        //     current_a * 1000.0f,
        //     power_w);

        // StatusLightsColor color = {.r = 127, .g = 30, .b = 30};

        // status_lights_notification(status_lights, StatusLightsTypeNet, color);
        // status_lights_notification(status_lights, StatusLightsTypePower, (StatusLightsColor){.r = 0, .g = 255, .b = 0});
        // status_lights_notification(status_lights, StatusLightsTypeBatteryOutline, (StatusLightsColor){.r = 0, .g = 0, .b = 255});
        // status_lights_notification(status_lights, StatusLightsTypeUsbWatt4, color);

        // furi_delay_ms(500);
        // status_lights_notification(status_lights, StatusLightsTypeNet, (StatusLightsColor){.r = 0, .g = 0, .b = 0});
        // furi_delay_ms(100);
        // status_lights_notification(status_lights, StatusLightsTypePower, (StatusLightsColor){.r = 0, .g = 0, .b = 0});
        // furi_delay_ms(100);
        // status_lights_notification(status_lights, StatusLightsTypeBatteryOutline, (StatusLightsColor){.r = 0, .g = 0, .b = 0});
        // furi_delay_ms(100);
        // status_lights_notification(status_lights, StatusLightsTypeUsbWatt4, (StatusLightsColor){.r = 0, .g = 0, .b = 0});

        // furi_delay_ms(500);

        // status_lights_notification(status_lights, StatusLightsTypeNet, color);
        // status_lights_notification(status_lights, StatusLightsTypeBatteryWatt1, (StatusLightsColor){.r = 0, .g = 255, .b = 0});
        // status_lights_notification(status_lights, StatusLightsTypeBatteryOutline, (StatusLightsColor){.r = 0, .g = 0, .b = 255});
        // status_lights_notification(status_lights, StatusLightsTypeUsbWatt3, color);
        // furi_delay_ms(500);
        // status_lights_notification(status_lights, StatusLightsTypeLine1Off, (StatusLightsColor){.r = 0, .g = 0, .b = 0});
        // furi_delay_ms(500);
        // status_lights_notification(status_lights, StatusLightsTypeLine2Off, (StatusLightsColor){.r = 0, .g = 0, .b = 0});
        // furi_delay_ms(500);
        // status_lights_notification(status_lights, StatusLightsTypeLine3Off, (StatusLightsColor){.r = 0, .g = 0, .b = 0});
        // furi_delay_ms(500);

        // status_lights_notification(status_lights, StatusLightsTypeEth2, color);
        // status_lights_notification(status_lights, StatusLightsTypeBatteryWatt4, (StatusLightsColor){.r = 0, .g = 255, .b = 0});
        // status_lights_notification(status_lights, StatusLightsTypeBatteryWatt1, (StatusLightsColor){.r = 0, .g = 0, .b = 255});
        // status_lights_notification(status_lights, StatusLightsTypeUsbWatt2, color);
        // furi_delay_ms(500);
        // status_lights_notification(status_lights, StatusLightsTypeLineAllOff, (StatusLightsColor){.r = 0, .g = 0, .b = 0});
        // furi_delay_ms(500);
    }
    furi_crash();
}
