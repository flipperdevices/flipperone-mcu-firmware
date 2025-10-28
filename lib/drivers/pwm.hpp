#pragma once
#include "math.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "log.hpp"
#include "hardware/clocks.h"

template <uint32_t pin, size_t bits = 12, size_t clock_div = 1, bool invert = false>
class PWMOutput {
public:
    void init(void) {
        gpio_set_function(pin, GPIO_FUNC_PWM);
        gpio_set_drive_strength(pin, GPIO_DRIVE_STRENGTH_2MA);
        gpio_set_slew_rate(pin, GPIO_SLEW_RATE_SLOW);
        gpio_disable_pulls(pin);
        gpio_set_input_enabled(pin, false);

        slice_num = pwm_gpio_to_slice_num(pin);
        channel_num = pwm_gpio_to_channel(pin);
        float freq = (float)clock_get_hz(clk_sys) / (float)clock_div / (1 << bits);
        Log::info("PWM %d: slice_num: %ld, channel_num: %ld, frequency: %.2f kHz", pin, slice_num, channel_num, freq / 1000.0f);

        // Set the PWM clock divider
        pwm_set_clkdiv_int_frac4(slice_num, clock_div, 0);

        // Set the PWM wrap value
        pwm_set_wrap(slice_num, max_value);
        pwm_set_enabled(slice_num, true);
    }

    void pwm(float value) {
        uint32_t pwm_value = (uint32_t)roundf(value * (float)max_value);
        if(pwm_value > max_value) {
            pwm_value = max_value;
        } else if(pwm_value < 0) {
            pwm_value = 0;
        }

        if(invert) {
            pwm_value = max_value - pwm_value; // Invert the PWM value
        }

        Log::info("PWM %d: value: %d", pin, pwm_value);
        pwm_set_chan_level(slice_num, channel_num, pwm_value);
    }

private:
    uint32_t slice_num;
    uint32_t channel_num;
    uint32_t max_value = (1 << bits);
};
