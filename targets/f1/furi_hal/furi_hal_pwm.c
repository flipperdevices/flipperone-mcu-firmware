#include <furi_hal_pwm.h>
#include <furi_hal.h>
#include <furi_hal_gpio.h>
#include <hardware/pwm.h>
#include <hardware/clocks.h>

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
        "PWM %d: slice_num: %ld, channel_num: %ld, frequency: %.2f kHz, div: %u",
        gpio->pin,
        instance->slice_num,
        instance->channel_num,
        freq_temp / 1000.0f,
        div_value);

    // Set the PWM clock divider
    pwm_set_clkdiv_int_frac4(instance->slice_num, div_value, 0);

    // Set the PWM wrap value
    pwm_set_wrap(instance->slice_num, instance->max_value);

    // Apply polarity inversion in hardware (CSR A_INV/B_INV) instead of
    // inverting the level in software. Preserve the other channel's polarity
    // bit in case the slice is shared with another PWM instance.
    const uint32_t csr = pwm_hw->slice[instance->slice_num].csr;
    pwm_set_output_polarity(
        instance->slice_num,
        instance->channel_num == 0 ? invert : (bool)(csr & PWM_CH0_CSR_A_INV_BITS),
        instance->channel_num == 1 ? invert : (bool)(csr & PWM_CH0_CSR_B_INV_BITS));

    pwm_set_enabled(instance->slice_num, true);
    return instance;
}

void furi_hal_pwm_deinit(FuriHalPwm* instance) {
    furi_check(instance);

    pwm_set_enabled(instance->slice_num, false);
    // Clear this channel's inversion bit, preserving the other channel's.
    const uint32_t csr = pwm_hw->slice[instance->slice_num].csr;
    pwm_set_output_polarity(
        instance->slice_num,
        instance->channel_num == 0 ? false : (bool)(csr & PWM_CH0_CSR_A_INV_BITS),
        instance->channel_num == 1 ? false : (bool)(csr & PWM_CH0_CSR_B_INV_BITS));
    furi_hal_gpio_init_ex(instance->gpio, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);

    free(instance);
}

void furi_hal_pwm_set_duty_cycle(FuriHalPwm* instance, uint32_t value) {
    furi_check(instance);

    if(value > instance->max_value) {
        value = instance->max_value;
    }

    // Polarity is handled by the slice's A_INV/B_INV hardware bit set at
    // init, so the level is written as-is.
    pwm_set_chan_level(instance->slice_num, instance->channel_num, value);
}
