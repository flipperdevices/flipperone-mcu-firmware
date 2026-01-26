#include <furi_hal_i2c_config.h>
#include <furi_hal_resources.h>
#include <drivers/tca6416a/tca6416a.h>

#include "furi_bsp_expander.h"

typedef struct {
    Tca6416a* handle;
    FuriCallback callback;
} Expander;

static Expander* expander_control = NULL;

void furi_bsp_expander_init(void) {
    furi_check(expander_control == NULL);
    expander_control = malloc(sizeof(Expander));
    expander_control->handle = tca6416a_init(&furi_hal_i2c_handle_internal, &gpio_expander_reset, &gpio_expander_int, TCA6416A_ADDRESS_A0);
    tca6416a_write_mode(expander_control->handle, InputKeyMask);
}

uint16_t furi_bsp_expander_control_read_buttons(void) {
    furi_assert(expander_control != NULL);
    return tca6416a_read_input(expander_control->handle);
}

void furi_bsp_expander_control_attach_buttons_callback(FuriCallback callback, void* context) {
    furi_check(callback != NULL);
    furi_check(expander_control != NULL);
    furi_check(expander_control->callback == NULL);
    tca6416a_set_input_callback(expander_control->handle, callback, context);
}

void furi_bsp_expander_control_led_power(uint16_t led_mask) {
    furi_check(expander_control != NULL);
    tca6416a_write_output(expander_control->handle, led_mask & StatusLedPowerMask);
}
