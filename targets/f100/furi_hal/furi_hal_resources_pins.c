#include "furi_hal_gpio.h"
#include <furi_hal_resources.h>

const GpioPin gpio_display_ctrl = {.pin = 35};
const GpioPin gpio_display_sda = {.pin = 19};
const GpioPin gpio_display_scl = {.pin = 18};
const GpioPin gpio_display_reset = {.pin = 12};
const GpioPin gpio_display_dc = {.pin = 13};
const GpioPin gpio_display_cs = {.pin = 17};

const GpioPin gpio_key1 = {.pin = 15};
const GpioPin gpio_key2 = {.pin = 14};
const GpioPin gpio_key3 = {.pin = 8};
const GpioPin gpio_key4 = {.pin = 7};
const GpioPin gpio_key5 = {.pin = 11};
const GpioPin gpio_key_sw = {.pin = 10};
const GpioPin gpio_key_up = {.pin = 9};
const GpioPin gpio_key_left = {.pin = 16};
const GpioPin gpio_key_center = {.pin = 6};
const GpioPin gpio_key_right = {.pin = 5};
const GpioPin gpio_key_down = {.pin = 4};
const GpioPin gpio_key_back = {.pin = 20};

const GpioPin gpio_normal_black = {.pin = 21};
const GpioPin gpio_bat_charging = {.pin = 24};
const GpioPin gpio_bat_charge_adc = {.pin = 29};
const GpioPin gpio_pico_first_adc = {.pin = 26};

const GpioPin gpio_pico_led = {.pin = 25};
const GpioPin gpio_status_led_line1 = {.pin = 2};
// const GpioPin gpio_status_led_line2 = {.pin = 2};
// const GpioPin gpio_status_led_line3 = {.pin = 2};

const GpioPinRecord gpio_pins[] = {};

const size_t gpio_pins_count = COUNT_OF(gpio_pins);
