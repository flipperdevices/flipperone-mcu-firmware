#pragma once

#include "furi_hal_gpio.h"

typedef struct FuriHalPwm FuriHalPwm;

#ifdef __cplusplus
extern "C" {
#endif

FuriHalPwm* furi_hal_pwm_init(const GpioPin* gpio, size_t bits, size_t clock_div, bool invert);
void furi_hal_pwm_deinit(FuriHalPwm* instance);
void furi_hal_pwm_set_duty_cycle(FuriHalPwm* instance, uint32_t value);
#ifdef __cplusplus
}
#endif
