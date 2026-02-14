#pragma once
#include <stdint.h>
#include <toolbox/furi_callback.h>
#include <furi_hal_resources.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Init all expander related hardware
 */
void furi_bsp_expander_init(void);

/** Returns the current state of buttons from the expander
 * @return InputKey - bitmask of button states
 */
InputKey furi_bsp_expander_control_read_buttons(void);

/** Attach a callback for button events
 * @param callback - function to call on button events
 * @param context - context to pass to the callback
 */
void furi_bsp_expander_control_attach_buttons_callback(FuriCallback callback, void* context);

/** Control power to status LED lines
 * @param led_mask - bitmask of LED lines to power on
 */
void furi_bsp_expander_control_led_power(StatusLedPower led_mask);

/** Returns the current state of inputs from the main expander
 * @return InputExpMain - bitmask of input states
 */
InputExpMain furi_bsp_expander_main_read_input(void);

/** Control outputs on the main expander
 * @param output_mask - bitmask of outputs to set high
 */
void furi_bsp_expander_main_write_output(OutputExpMain output_mask);

/** Returns the current state of outputs from the main expander
 * @return OutputExpMain - bitmask of output states
 */
OutputExpMain furi_bsp_expander_main_read_output(void);

#ifdef __cplusplus
}
#endif
