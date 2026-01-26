#pragma once
#include <stdint.h>
#include <toolbox/furi_callback.h>

#ifdef __cplusplus
extern "C" {
#endif

void furi_bsp_expander_init(void);

uint16_t furi_bsp_expander_control_read_buttons(void);

void furi_bsp_expander_control_attach_buttons_callback(FuriCallback callback, void* context);

void furi_bsp_expander_control_led_power(uint16_t led_mask);

#ifdef __cplusplus
}
#endif
