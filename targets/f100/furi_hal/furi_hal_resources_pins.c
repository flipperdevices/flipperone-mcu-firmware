#include "furi_hal_gpio.h"
#include <furi_hal_resources.h>

const GpioPin gpio_qspi0_cs1 = {.pin = 0};
const GpioPin gpio_nboot_disable = {.pin = 1};

const GpioPin gpio_i2cx_sda = {.pin = 10};
const GpioPin gpio_i2cx_scl = {.pin = 11};

const GpioPin gpio_display_te = {.pin = 5};
const GpioPin gpio_display_ctrl = {.pin = 6};
const GpioPin gpio_display_reset = {.pin = 12};
const GpioPin gpio_display_d2 = {.pin = 14};
const GpioPin gpio_display_d1 = {.pin = 15};
const GpioPin gpio_display_d0 = {.pin = 16};
const GpioPin gpio_display_cs = {.pin = 17};
const GpioPin gpio_display_scl = {.pin = 18};
const GpioPin gpio_display_sda = {.pin = 19};
const GpioPin gpio_display_vci_en = {.pin = 24};
//spi mode pins
const GpioPin gpio_display_dc = {.pin = 13};

const GpioPin gpio_haptic_en = {.pin = 7};
const GpioPin gpio_haptic_pwm = {.pin = 8};

const GpioPin gpio_touchpad_rdy = {.pin = 9};

const GpioPin gpio_i2c0_sda = {.pin = 20};
const GpioPin gpio_i2c0_scl = {.pin = 21};
const GpioPin gpio_i2c1_sda = {.pin = 22};
const GpioPin gpio_i2c1_scl = {.pin = 23};

const GpioPin gpio_status_led_line1 = {.pin = 29};
const GpioPin gpio_status_led_line2 = {.pin = 30};
const GpioPin gpio_status_led_line3 = {.pin = 31};

const GpioPin gpio_audio_hp_detect = {.pin = 42};
const GpioPin gpio_audio_key = {.pin = 47};

const GpioPin gpio_uart1_tx = {.pin = 36};
const GpioPin gpio_uart1_rx = {.pin = 37};

const GpioPin gpio_expander_reset = {.pin = 43};
const GpioPin gpio_expander_int = {.pin = 46};

//interconnect
const GpioPin gpio_cpu_spi_cs = {.pin = 25};
const GpioPin gpio_cpu_spi_sck = {.pin = 26};
const GpioPin gpio_cpu_spi_miso = {.pin = 27}; //DC
const GpioPin gpio_cpu_spi_mosi = {.pin = 28};

const GpioPin gpio_cpu_audio_hp_int = {.pin = 34};
const GpioPin gpio_cpu_int = {.pin = 35};

const GpioPin gpio_cpu_i3c0_sda = {.pin = 2};
const GpioPin gpio_cpu_i3c0_scl = {.pin = 3};

const GpioPin gpio_cpu_adc_in0_boot = {.pin = 4};

const GpioPin gpio_mcu_gpio0 = {.pin = 40};
const GpioPin gpio_mcu_gpio1 = {.pin = 41};

const GpioPin gpio_main_board_reset = {.pin = 38};
const GpioPin gpio_main_expander_int = {.pin = 39};

const GpioPin gpio_uart0_tx = {.pin = 32};
const GpioPin gpio_uart0_rx = {.pin = 33};

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
    {.key = InputKeyPtt, .inverted = true, .name = "PTT"},
    {.key = InputKeySw, .inverted = true, .name = "Sw"},
};

const size_t input_pins_count = COUNT_OF(input_pins);
