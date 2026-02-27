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
    InputKey2 = (1 << 0),
    InputKey1 = (1 << 1),
    InputKey3 = (1 << 2), //,InputKeyPower
    InputKey4 = (1 << 3),
    InputKey5 = (1 << 4),
    InputKeySw = (1 << 5),
    InputKeyBack = (1 << 6),
    InputKeyDown = (1 << 7),
    InputKeyRight = (1 << 8),
    InputKeyOk = (1 << 9),
    InputKeyLeft = (1 << 10),
    InputKeyUp = (1 << 11),
    InputKeyPtt = (1 << 12),

    InputKeyMask = (0x1FFF),
} InputKey;

typedef enum {
    StatusLedPowerLine3 = (1 << 13),
    StatusLedPowerLine2 = (1 << 14),
    StatusLedPowerLine1 = (1 << 15),
    StatusLedPowerMask = (0xE000),
} StatusLedPower;

typedef enum {
    InputExpMainGpio5v0Flt = (1 << 0),
    InputExpMainGpio3v3Flt = (1 << 1),
    InputExpMainBq25792Int = (1 << 2),
    InputExpMainFusb302Int = (1 << 3),
    InputExpMainMuxVconnFault = (1 << 4),
    InputExpMainTypeCUpSwPg = (1 << 5),
    InputExpMainTypeAUpSwPg = (1 << 6),
    InputExpMainExpander7 = (1 << 7),

    InputExpMainInputMask = (0x00FF),
} InputExpMain;

typedef enum {
    OutputExpMainUsb20Sel = (1 << 8),
    OutputExpMainHubPwrEn = (1 << 9),
    OutputExpMainTypeAUpSwEn = (1 << 10),
    OutputExpMainVcc5v0DevS0En = (1 << 11),
    OutputExpMainVcc5v0SysS5En = (1 << 12),
    OutputExpMainGpio5v0En = (1 << 13),
    OutputExpMainGpio3v3En = (1 << 14),
    OutputExpMainExpander17 = (1 << 15),
    OutputExpMainMask = (0xFF00),
} OutputExpMain;

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

extern const GpioPin gpio_qspi0_cs1;
extern const GpioPin gpio_nboot_disable;

extern const GpioPin gpio_i2cx_sda;
extern const GpioPin gpio_i2cx_scl;

extern const GpioPin gpio_display_te;
extern const GpioPin gpio_display_ctrl;
extern const GpioPin gpio_display_reset;
extern const GpioPin gpio_display_d2;
extern const GpioPin gpio_display_d1;
extern const GpioPin gpio_display_d0;
extern const GpioPin gpio_display_cs;
extern const GpioPin gpio_display_scl;
extern const GpioPin gpio_display_sda;
extern const GpioPin gpio_display_vci_en;
//spi mode pins
extern const GpioPin gpio_display_dc;

extern const GpioPin gpio_haptic_en;
extern const GpioPin gpio_haptic_pwm;

extern const GpioPin gpio_touchpad_rdy;

extern const GpioPin gpio_i2c0_sda;
extern const GpioPin gpio_i2c0_scl;
extern const GpioPin gpio_i2c1_sda;
extern const GpioPin gpio_i2c1_scl;

extern const GpioPin gpio_status_led_line1;
extern const GpioPin gpio_status_led_line2;
extern const GpioPin gpio_status_led_line3;

extern const GpioPin gpio_audio_hp_detect;
extern const GpioPin gpio_audio_key;

extern const GpioPin gpio_uart1_tx;
extern const GpioPin gpio_uart1_rx;

extern const GpioPin gpio_expander_reset;
extern const GpioPin gpio_expander_int;
//interconnect
extern const GpioPin gpio_cpu_spi_cs;
extern const GpioPin gpio_cpu_spi_sck;
extern const GpioPin gpio_cpu_spi_miso;
extern const GpioPin gpio_cpu_spi_mosi;

extern const GpioPin gpio_cpu_audio_hp_int;
extern const GpioPin gpio_cpu_int;

extern const GpioPin gpio_cpu_i3c0_sda;
extern const GpioPin gpio_cpu_i3c0_scl;

extern const GpioPin gpio_cpu_adc_in0_boot;

extern const GpioPin gpio_mcu_gpio0;
extern const GpioPin gpio_mcu_gpio1;

extern const GpioPin gpio_main_board_reset;
extern const GpioPin gpio_main_expander_int;

extern const GpioPin gpio_uart0_tx;
extern const GpioPin gpio_uart0_rx;

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
