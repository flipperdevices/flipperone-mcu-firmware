#pragma once
#include <stdint.h>
#include <toolbox/furi_callback.h>
#include <furi_hal_resources.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FuriBspControlExpanderMainOn,
    FuriBspControlExpanderMainOff,
} FuriBspControlExpanderMain;

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

/** Control the main expander interrupt handler
 * @param control - whether to turn on or off the interrupt handler
 */
void furi_bsp_expander_main_set_control(FuriBspControlExpanderMain control);

/** Get the current state of the main expander interrupt handler
 * @return FuriBspControlExpanderMain - whether the interrupt handler is on or off
 */
FuriBspControlExpanderMain furi_bsp_expander_main_get_control_state(void);

/** Attach callbacks for main expander input events
 * @param callback - function to call on GPIO 5V0 fault events
 * @param context - context to pass to the callback
 */
void furi_bsp_expander_main_attach_gpio_5v0_flt_callback(FuriCallback callback, void* context);

/** Attach callbacks for main expander input events
 * @param callback - function to call on GPIO 3V3 fault events
 * @param context - context to pass to the callback
 */
void furi_bsp_expander_main_attach_gpio_3v3_flt_callback(FuriCallback callback, void* context);

/** Attach callbacks for main expander input events
 * @param callback - function to call on BQ25798 interrupt events
 * @param context - context to pass to the callback
 */
void furi_bsp_expander_main_attach_bq25798_callback(FuriCallback callback, void* context);

/** Attach callbacks for main expander input events
 * @param callback - function to call on FUSB302 interrupt events
 * @param context - context to pass to the callback
 */
void furi_bsp_expander_main_attach_fusb302_callback(FuriCallback callback, void* context);

/** Attach callbacks for main expander input events
 * @param callback - function to call on MUX VCONN fault events
 * @param context - context to pass to the callback
 */
void furi_bsp_expander_main_attach_mux_vconn_fault_callback(FuriCallback callback, void* context);

/** Attach callbacks for main expander input events
 * @param callback - function to call on Type-C Up SW PG events
 * @param context - context to pass to the callback
 */
void furi_bsp_expander_main_attach_type_c_up_sw_pg_callback(FuriCallback callback, void* context);

/** Attach callbacks for main expander input events
 * @param callback - function to call on Type-A Up SW PG events
 * @param context - context to pass to the callback
 */
void furi_bsp_expander_main_attach_type_a_up_sw_pg_callback(FuriCallback callback, void* context);

/** Attach callbacks for main expander input events
 * @param callback - function to call on Expander 7 events
 * @param context - context to pass to the callback
 */
void furi_bsp_expander_main_attach_expander7_callback(FuriCallback callback, void* context);

void furi_bsp_main_reset(void);

#ifdef __cplusplus
}
#endif
