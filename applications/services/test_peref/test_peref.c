#include "test_peref.h"
#include <furi.h>

#include <furi_hal_resources.h>
#include <furi_hal_gpio.h>
#include <drivers/display/display_jd9853.h>

#include <furi_hal_pwm.h>
#include <drivers/ws2812/ws2812.h>
#include <strings.h>

#include <furi_hal_i2c.h>
#include <furi_hal_i2c_types_i.h>

#define tag "TestPerefSrv"

static void key1_callback(void* ctx) {
    //printf("Key1 pressed!");
    furi_hal_gpio_write(&gpio_pico_led, true);
    furi_hal_gpio_write(&gpio_pico_led, false);
}

int32_t test_peref_srv(void* p) {
    UNUSED(p);

    furi_log_set_level(FuriLogLevelDebug);
    FURI_LOG_T("tag", "Trace");
    FURI_LOG_D("tag", "Debug");
    FURI_LOG_I("tag", "Info");
    FURI_LOG_W("tag", "Warning");
    FURI_LOG_E("tag", "Error");

    furi_hal_gpio_init_simple(&gpio_pico_led, GpioModeOutputPushPull);
    furi_hal_gpio_init_simple(&gpio_key1, GpioModeInput);
    furi_hal_gpio_add_int_callback(&gpio_key1, GpioConditionFall, key1_callback, NULL);

    FuriHalPwm* pwm = furi_hal_pwm_init(&gpio_key_back, 8, 200000, false);
    uint8_t duty = 0;

    GpioPin* ws2812_pins = (GpioPin*)malloc(sizeof(GpioPin) * 1);
    ws2812_pins[0] = gpio_status_led_line1;
    Ws2812* ws2812 = ws2812_init(ws2812_pins, 1);
    free(ws2812_pins);

    DisplayJd9853* display = display_jd9853_init();
    display_jd9853_fill(display, 0x00, 0x00, 0xFF); // Fill blue
    uint8_t index_led = 0;

    FuriHalI2cHandle i2c_handle = {.id = FuriHalI2cIdI2c0, .in_use = true};
    furi_hal_i2c_master_init(&i2c_handle, 400000);

    while(true) {
        furi_hal_gpio_write(&gpio_pico_led, true);
        furi_delay_ms(10);
        furi_hal_gpio_write(&gpio_pico_led, false);
        furi_delay_ms(10);
        display_jd9853_fill(display, 0x00, 0x00, 0xFF); // Fill blue
        furi_delay_ms(10);
        display_jd9853_fill(display, 0x00, 0xFF, 0xFF); // Fill cyan
        furi_delay_ms(10);
        display_jd9853_fill(display, 0xFF, 0x00, 0xFF); // Fill magenta
        furi_delay_ms(10);
        display_jd9853_fill(display, 0xFF, 0xFF, 0x00); // Fill yellow
        furi_delay_ms(10);
        display_jd9853_fill(display, 0x00, 0x00, 0x00); // Fill black

        furi_hal_pwm_set_duty_cycle(pwm, duty);
        duty += 5;
        for(size_t i = 0; i < 29; i++) {
            if(index_led == i) {
                // ws2812_put_pixel_rgb(ws2812, 0, duty, 0, 255 - duty);
                // ws2812_put_pixel_rgb(ws2812, 0, 255 - duty, 0, duty);
                // ws2812_put_pixel_rgb(ws2812, 0, 255 - duty, duty, 0);
                ws2812_put_pixel_rgb(ws2812, 0, 255, 0, 0);
                ws2812_put_pixel_rgb(ws2812, 0, 0, 255, 0);
                ws2812_put_pixel_rgb(ws2812, 0, 0, 0, 255);
            } else {
                ws2812_put_pixel_rgb(ws2812, 0, 0, 0, 0);
            }
        }
        index_led++;
        if(index_led >= 30) {
            index_led = 0;
        }
        furi_delay_ms(10);
        furi_hal_i2c_bus_scan_print(&i2c_handle);
    }
    furi_crash();
}