/**
 * @file furi_hal_resources.h
 * @brief Hardware resources API
 */
#pragma once

#include <furi.h>
#include <furi_hal_gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    InputKeyUp,
    InputKeyDown,
    InputKeyRight,
    InputKeyLeft,
    InputKeyOk,
    InputKeyBack,
    InputKeyRtt,
    InputKeySw,
    InputKey1,
    InputKey2,
    InputKeyPower, //InputKey3,
    InputKey4,
    InputKey5,

    InputKeyMAX, /**< Special value, don't use it */
} InputKey;

typedef struct {
    const GpioPin* pin;
    const char* name;
    const uint8_t number;
    const bool debug;
} GpioPinRecord;

extern const GpioPin gpio_display_ctrl;
extern const GpioPin gpio_display_sda;
extern const GpioPin gpio_display_scl;
extern const GpioPin gpio_display_reset;
extern const GpioPin gpio_display_dc;
extern const GpioPin gpio_display_cs;
extern const GpioPin gpio_key1;
extern const GpioPin gpio_key2;
extern const GpioPin gpio_key3;
extern const GpioPin gpio_key4;
extern const GpioPin gpio_key5;
extern const GpioPin gpio_key_sw;
extern const GpioPin gpio_key_up;
extern const GpioPin gpio_key_left;
extern const GpioPin gpio_key_center;
extern const GpioPin gpio_key_right;
extern const GpioPin gpio_key_down;
extern const GpioPin gpio_key_back;
extern const GpioPin gpio_normal_black;
extern const GpioPin gpio_bat_charging;
extern const GpioPin gpio_bat_charge_adc;
extern const GpioPin gpio_pico_first_adc;

extern const GpioPin gpio_pico_led;
extern const GpioPin gpio_status_led_line1;
// extern const GpioPin gpio_status_led_line2;
// extern const GpioPin gpio_status_led_line3;

extern const GpioPinRecord gpio_pins[];
extern const size_t gpio_pins_count;

void furi_hal_resources_init_early(void);

void furi_hal_resources_deinit_early(void);

void furi_hal_resources_init(void);

#ifdef __cplusplus
}
#endif
