#include <furi_hal_pwm.h>
#include <furi_hal.h>
#include <furi_hal_gpio.h>
#include <hardware/pwm.h>
#include "hardware/clocks.h"

#define TAG "FuriHalPwm"

struct FuriHalPwm {
    const GpioPin* gpio;
    uint32_t slice_num;
    uint32_t channel_num;
    uint32_t max_value;
    bool invert;
};

FuriHalPwm* furi_hal_pwm_init(const GpioPin* gpio, size_t bits, size_t freq_hz, bool invert) {
    furi_check(gpio->pin <= NUM_BANK0_GPIOS);
    furi_check(bits > 0 && bits <= 16);

    FuriHalPwm* instance = malloc(sizeof(FuriHalPwm));
    furi_hal_gpio_init_ex(gpio, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow, GpioAltFn4Pwm);
    furi_hal_gpio_set_drive_strength(gpio, GpioDriveStrengthMedium);

    instance->gpio = gpio;
    instance->slice_num = pwm_gpio_to_slice_num(gpio->pin);
    instance->channel_num = pwm_gpio_to_channel(gpio->pin);
    instance->max_value = (1 << bits);
    instance->invert = invert;

    float div = (float)clock_get_hz(clk_sys) / ((float)freq_hz * instance->max_value);

    uint8_t div_value = (uint8_t)roundf(div);
    float freq_temp = (float)clock_get_hz(clk_sys) / (float)(div_value) / instance->max_value;
    FURI_LOG_D(
        TAG,
        "PWM %d: slice_num: %ld, channel_num: %ld, frequency: %.2f kHz, div: %f",
        gpio->pin,
        instance->slice_num,
        instance->channel_num,
        freq_temp / 1000.0f,
        div_value);

    // Set the PWM clock divider
    pwm_set_clkdiv_int_frac4(instance->slice_num, div_value, 0);

    // Set the PWM wrap value
    pwm_set_wrap(instance->slice_num, instance->max_value);
    pwm_set_enabled(instance->slice_num, true);
    return instance;
}

void furi_hal_pwm_deinit(FuriHalPwm* instance) {
    furi_check(instance);

    pwm_set_enabled(instance->slice_num, false);
    furi_hal_gpio_init_ex(instance->gpio, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);

    free(instance);
}

void furi_hal_pwm_set_duty_cycle(FuriHalPwm* instance, uint32_t value) {
    furi_check(instance);

    if(value > instance->max_value) {
        value = instance->max_value;
    } else if(value < 0) {
        value = 0;
    }

    if(instance->invert) {
        value = instance->max_value - value; // Invert the PWM value
    }

    FURI_LOG_D(TAG, "PWM %d: value: %d", instance->gpio->pin, value);
    pwm_set_chan_level(instance->slice_num, instance->channel_num, value);
}
