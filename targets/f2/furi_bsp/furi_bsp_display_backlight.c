#include <furi.h>
#include <furi_hal_resources.h>
#include <furi_hal_pwm.h>

#include <furi_bsp_display_backlight.h>

#define TAG "Display Backlight"

#define BACKLIGHT_PWM_RESOLUTION 8 // 8-bit PWM for backlight
#define BACKLIGHT_PWM_FREQ_HZ    40000 // 40kHz PWM for backlight

// typedef struct {
//     uint8_t brightness;
//     FuriHalPwm* backlight_pwm;
// } FuriBspDisplayBacklight;

// static FuriBspDisplayBacklight handle_backlight = {0, NULL};

static FuriHalPwm* backlight_pwm = NULL;

void furi_bsp_display_backlight_init(void) {
}

void furi_bsp_display_backlight_set_brightness(uint8_t brightness) {
    FURI_LOG_D(TAG, "Setting backlight brightness to %u", brightness);

    if(brightness == 0) {
        if(backlight_pwm) {
            furi_hal_pwm_set_duty_cycle(backlight_pwm, 0);
            furi_hal_pwm_deinit(backlight_pwm);
            backlight_pwm = NULL;
        }
    } else {
        uint32_t max_value = (1 << BACKLIGHT_PWM_RESOLUTION) - 1;
        furi_check(brightness <= max_value);
        if(!backlight_pwm) {
            // To enable the device, the CTRL signal must be high for 500 µs.
            // The PWM signal can then be applied with a pulse width (tp)
            // greater or smaller than tON. To force the device into shutdown mode,
            // the CTRL signal must be low for at least 32 ms.
            // Requiring the CTRL pin to be low for 32 mS before the device enters
            // shutdown allows for PWM dimming frequencies as low as 100 Hz.
            // The device is enabled again when a CTRL signal is high for a period of 500 µs minimum.
            // TODO: rework for new HW revision
            furi_hal_gpio_init(&gpio_display_backlight_pwm, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
            furi_hal_gpio_write(&gpio_display_backlight_en, true);
            backlight_pwm = furi_hal_pwm_init(&gpio_display_backlight_en, BACKLIGHT_PWM_RESOLUTION, BACKLIGHT_PWM_FREQ_HZ, false);
            furi_hal_pwm_set_duty_cycle(backlight_pwm, max_value);
            furi_delay_us(500);
            furi_hal_pwm_set_duty_cycle(backlight_pwm, 2);
            furi_hal_gpio_init(&gpio_display_backlight_pwm, GpioModeInput, GpioPullNo, GpioSpeedLow);
            FURI_LOG_D(TAG, "Backlight PWM initialized with resolution %u bits and frequency %u Hz", BACKLIGHT_PWM_RESOLUTION, BACKLIGHT_PWM_FREQ_HZ);
        }

        furi_hal_pwm_set_duty_cycle(backlight_pwm, brightness);
    }
}
