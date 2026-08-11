#include <furi.h>
#include <furi_hal_resources.h>
#include <furi_hal_pwm.h>

#include <furi_bsp_display_backlight.h>

#define BACKLIGHT_PWM_RESOLUTION 8 // 8-bit PWM for backlight
#define BACKLIGHT_PWM_FREQ_HZ    40000 // 40kHz PWM for backlight

static FuriHalPwm* backlight_pwm = NULL;

void furi_bsp_display_backlight_init(void) {
    furi_hal_gpio_init(&gpio_display_backlight_en, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
}

void furi_bsp_display_backlight_set_brightness(uint8_t brightness) {
    // TODO: smooth transition using DMA + PWM

    brightness = 255 - brightness; // Invert brightness for hardware

    FURI_LOG_I("Display Backlight", "Setting brightness to %u", brightness);

    if(brightness == 0) {
        if(backlight_pwm) {
            furi_hal_gpio_write(&gpio_display_backlight_en, false);
            furi_hal_pwm_set_duty_cycle(backlight_pwm, 0);
            furi_hal_pwm_deinit(backlight_pwm);
            backlight_pwm = NULL;
        }
    } else {
        uint32_t max_value = (1 << BACKLIGHT_PWM_RESOLUTION) - 1;
        uint32_t duty_cycle = (brightness * max_value) / 255;
        if(!backlight_pwm) {
            // To enable the device, the CTRL signal must be high for 500 µs.
            // The PWM signal can then be applied with a pulse width (tp)
            // greater or smaller than tON. To force the device into shutdown mode,
            // the CTRL signal must be low for at least 32 ms.
            // Requiring the CTRL pin to be low for 32 mS before the device enters
            // shutdown allows for PWM dimming frequencies as low as 100 Hz.
            // The device is enabled again when a CTRL signal is high for a period of 500 µs minimum.
            // TODO: rework for new HW revision
            backlight_pwm = furi_hal_pwm_init(&gpio_display_backlight_pwm, BACKLIGHT_PWM_RESOLUTION, BACKLIGHT_PWM_FREQ_HZ, false);
            furi_hal_gpio_write(&gpio_display_backlight_en, true);
            FURI_LOG_I(
                "Display Backlight", "Backlight PWM initialized with resolution %u bits and frequency %u Hz", BACKLIGHT_PWM_RESOLUTION, BACKLIGHT_PWM_FREQ_HZ);
        }

        FURI_LOG_I("Display Backlight", "Setting duty cycle to %lu", duty_cycle);
        furi_hal_pwm_set_duty_cycle(backlight_pwm, duty_cycle);
    }
}
