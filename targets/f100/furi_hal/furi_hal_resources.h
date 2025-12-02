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

/* Input Related Constants */
#define INPUT_DEBOUNCE_TICKS 4

typedef enum {
    InputKey1 = (1 << 0),
    InputKey2 = (1 << 1),
    InputKey3 = (1 << 2), //,InputKeyPower
    InputKey4 = (1 << 3),
    InputKey5 = (1 << 4),
    InputKeyBack = (1 << 5),
    InputKeyUp = (1 << 6),
    InputKeyLeft = (1 << 7),
    InputKeyOk = (1 << 8),
    InputKeyRight = (1 << 9),
    InputKeyDown = (1 << 10),
    InputKeySw = (1 << 11),
    InputKeyRtt = (1 << 12),
    InputKeyMask = (0x1FFF),
} InputKey;

typedef enum {
    StatusLedPowerLine1 = (1 << 13),
    StatusLedPowerLine2 = (1 << 14),
    StatusLedPowerLine3 = (1 << 15),
    StatusLedPowerMask = (0xE000),
} StatusLedPower;

typedef struct {
    const InputKey key;
    const bool inverted;
    const char* name;
} InputPin;

typedef struct {
    const GpioPin* pin;
    const char* name;
    const uint8_t number;
    const bool debug;
} GpioPinRecord;

extern const GpioPin gpio_uart0_tx;
extern const GpioPin gpio_uart0_rx;
extern const GpioPin gpio_uart1_tx;
extern const GpioPin gpio_uart1_rx;

extern const GpioPin gpio_display_ctrl;
extern const GpioPin gpio_display_sda;
extern const GpioPin gpio_display_scl;
extern const GpioPin gpio_display_reset;
extern const GpioPin gpio_display_dc;
extern const GpioPin gpio_display_cs;
extern const GpioPin gpio_display_vci_en;
extern const GpioPin gpio_display_d0;
extern const GpioPin gpio_display_d1;
extern const GpioPin gpio_display_d2;
extern const GpioPin gpio_display_te;

extern const GpioPin gpio_cpu_spi_cs;
extern const GpioPin gpio_cpu_spi_sck;
extern const GpioPin gpio_cpu_spi_miso;
extern const GpioPin gpio_cpu_spi_mosi;

extern const GpioPin gpio_pico_led;
extern const GpioPin gpio_status_led_line1;
// extern const GpioPin gpio_status_led_line2;
// extern const GpioPin gpio_status_led_line3;
extern const GpioPin gpio_i2c0_sda;
extern const GpioPin gpio_i2c0_scl;
extern const GpioPin gpio_i2c1_sda;
extern const GpioPin gpio_i2c1_scl;

extern const GpioPin gpio_touchpad_rdy;
extern const GpioPin gpio_expander_reset;
extern const GpioPin gpio_expander_int;

extern const GpioPin gpio_haptic_en;
extern const GpioPin gpio_haptic_pwm;

extern const GpioPinRecord gpio_pins[];
extern const size_t gpio_pins_count;

extern const InputPin input_pins[];
extern const size_t input_pins_count;

void furi_hal_resources_init_early(void);

void furi_hal_resources_deinit_early(void);

void furi_hal_resources_init(void);

#ifdef __cplusplus
}
#endif
