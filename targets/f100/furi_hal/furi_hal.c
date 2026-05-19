#include <furi_hal.h>
#include <furi_hal_power.h>
#include <furi_hal_nvm.h>
#include <furi_hal_gpio.h>
#include <furi_hal_adc.h>

#define TAG "FuriHal"

void furi_hal_init_early(void) {
    furi_hal_nvm_init();
    furi_hal_os_init();
    furi_hal_i2c_init_control();
    furi_hal_i2c_init_main();
    furi_hal_i2c_init_cpu();
}

void furi_hal_deinit_early(void) {
    furi_hal_i2c_deinit_control();
    furi_hal_i2c_deinit_main();
    furi_hal_i2c_deinit_cpu();
}

void furi_hal_init(void) {
    furi_hal_gpio_interrupt_init();
    furi_hal_adc_init();

    furi_hal_otp_init();
}
