#pragma once
#include <stdint.h>
#include <toolbox/furi_callback.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Init all expander related hardware
 */
void furi_bsp_expander_init(void);

/** Returns the current state of buttons from the expander
 * @return uint16_t - bitmask of button states
 */
uint16_t furi_bsp_expander_control_read_buttons(void);

/** Attach a callback for button events
 * @param callback - function to call on button events
 * @param context - context to pass to the callback
 */
void furi_bsp_expander_control_attach_buttons_callback(FuriCallback callback, void* context);

/** Control power to status LED lines
 * @param led_mask - bitmask of LED lines to power on
 */
void furi_bsp_expander_control_led_power(uint16_t led_mask);

#ifdef __cplusplus
}
#endif
