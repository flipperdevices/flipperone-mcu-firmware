#include "furi_hal_gpio.h"
#include <furi_hal_resources.h>

const GpioPin gpio_uart0_tx = {.pin = 0};
const GpioPin gpio_uart0_rx = {.pin = 1};

const GpioPin gpio_uart1_tx = {.pin = -1};
const GpioPin gpio_uart1_rx = {.pin = -1};

const GpioPin gpio_display_ctrl = {.pin = 8};
const GpioPin gpio_display_sda = {.pin = 7};
const GpioPin gpio_display_scl = {.pin = 6};
const GpioPin gpio_display_reset = {.pin = 3};
const GpioPin gpio_display_dc = {.pin = 4};
const GpioPin gpio_display_cs = {.pin = 5};
const GpioPin gpio_display_vci_en = {.pin = -1};
const GpioPin gpio_display_d0 = {.pin = -1};
const GpioPin gpio_display_d1 = {.pin = -1};
const GpioPin gpio_display_d2 = {.pin = -1};
const GpioPin gpio_display_te = {.pin = 21};

const GpioPin gpio_key1 = {.pin = 15};
const GpioPin gpio_key2 = {.pin = 14};
const GpioPin gpio_key3 = {.pin = 13};
const GpioPin gpio_key4 = {.pin = 12};
const GpioPin gpio_key5 = {.pin = 11};
const GpioPin gpio_key_sw = {.pin = 10};
const GpioPin gpio_key_up = {.pin = 9};
const GpioPin gpio_key_left = {.pin = 16};
const GpioPin gpio_key_center = {.pin = 17};
const GpioPin gpio_key_right = {.pin = 18};
const GpioPin gpio_key_down = {.pin = 19};
const GpioPin gpio_key_back = {.pin = 20};

const GpioPin gpio_cpu_spi_cs = {.pin = -1};
const GpioPin gpio_cpu_spi_sck = {.pin = -1};
const GpioPin gpio_cpu_spi_miso = {.pin = -1};
const GpioPin gpio_cpu_spi_mosi = {.pin = -1};

//const GpioPin gpio_normal_black = {.pin = -1};
// const GpioPin gpio_bat_charging = {.pin = -1};
// const GpioPin gpio_bat_charge_adc = {.pin = -1};
// const GpioPin gpio_pico_first_adc = {.pin = -1};

const GpioPin gpio_pico_led = {.pin = -1};
const GpioPin gpio_status_led_line1 = {.pin = 2};
// const GpioPin gpio_status_led_line2 = {.pin = -1};
// const GpioPin gpio_status_led_line3 = {.pin = -1};

const GpioPin gpio_i2c0_sda = {.pin = -1};
const GpioPin gpio_i2c0_scl = {.pin = -1};
const GpioPin gpio_i2c1_sda = {.pin = 26};
const GpioPin gpio_i2c1_scl = {.pin = 27};

const GpioPin gpio_touchpad_rdy = {.pin = 22};

const GpioPin gpio_expander_reset = {.pin = -1};
const GpioPin gpio_expander_int = {.pin = -1};

const GpioPin gpio_haptic_en = {.pin = -1};
const GpioPin gpio_haptic_pwm = {.pin = -1};

const GpioPinRecord gpio_pins[] = {};
const size_t gpio_pins_count = COUNT_OF(gpio_pins);

const InputPin input_pins[] = {
    {.key = InputKey1, .inverted = true, .name = "Key1"},
    {.key = InputKey2, .inverted = true, .name = "Key2"},
    {.key = InputKey3, .inverted = true, .name = "Power"},
    {.key = InputKey4, .inverted = true, .name = "Key4"},
    {.key = InputKey5, .inverted = true, .name = "Key5"},
    {.key = InputKeyBack, .inverted = true, .name = "Back"},
    {.key = InputKeyUp, .inverted = true, .name = "Up"},
    {.key = InputKeyDown, .inverted = true, .name = "Down"},
    {.key = InputKeyRight, .inverted = true, .name = "Right"},
    {.key = InputKeyLeft, .inverted = true, .name = "Left"},
    {.key = InputKeyOk, .inverted = true, .name = "OK"},
    {.key = InputKeyRtt, .inverted = true, .name = "RTT"},
    {.key = InputKeySw, .inverted = true, .name = "Sw"},
};

const size_t input_pins_count = COUNT_OF(input_pins);
