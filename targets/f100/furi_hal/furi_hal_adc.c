#include <furi_hal_adc.h>
#include <furi.h>
#include <hardware/adc.h>

#define TAG "FuriHalAdc"

typedef uint32_t FuriHalAdcChannelEnable;

FuriHalAdcChannelEnable furi_hal_adc_channel_enable_mask = 0;

void furi_hal_adc_init(void) {
    adc_init();
    FURI_LOG_I(TAG, "Init OK");
}

void furi_hal_adc_deinit(void) {
    // Not Implemented
    FURI_LOG_I(TAG, "Deinit OK");
}

void furi_hal_adc_gpio_init(const GpioPin* gpio) {
    furi_check(gpio);
    furi_check((NUM_BANK0_GPIOS - gpio->pin) <= 8); // Ensure pin is within ADC range
    FuriHalAdcChannelEnable adc_channel = 8 - (NUM_BANK0_GPIOS - gpio->pin);
    furi_hal_adc_channel_enable_mask |= (1 << adc_channel);

    FURI_CRITICAL_ENTER();
    adc_gpio_init(gpio->pin);
    FURI_CRITICAL_EXIT();
}

uint16_t furi_hal_adc_read(const GpioPin* gpio) {
    furi_check(gpio);
    furi_check((NUM_BANK0_GPIOS - gpio->pin) <= 8); // Ensure pin is within ADC range
    FuriHalAdcChannelEnable adc_channel = 8 - (NUM_BANK0_GPIOS - gpio->pin);
    furi_check((furi_hal_adc_channel_enable_mask & (1 << adc_channel)) != 0);

    FURI_CRITICAL_ENTER();
    adc_select_input(adc_channel);
    uint16_t result = adc_read();
    FURI_CRITICAL_EXIT();

    return result;
}

float FURI_ALWAYS_INLINE furi_hal_adc_conversion_factor(void) {
    return 3.3f / (1 << 12);
}

float FURI_ALWAYS_INLINE furi_hal_adc_read_voltage(const GpioPin* gpio) {
    return furi_hal_adc_read(gpio) * furi_hal_adc_conversion_factor();
}