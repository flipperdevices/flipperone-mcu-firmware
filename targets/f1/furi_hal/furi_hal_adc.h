#pragma once

#include <furi_hal_gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

void furi_hal_adc_init(void);
void furi_hal_adc_deinit(void);
void furi_hal_adc_gpio_init(const GpioPin* gpio);
uint16_t furi_hal_adc_read(const GpioPin* gpio);
float furi_hal_adc_conversion_factor(void);
float furi_hal_adc_read_voltage(const GpioPin* gpio);

#ifdef __cplusplus
}
#endif
