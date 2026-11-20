#pragma once
#include <furi.h>
#include <furi_hal_resources.h>

/** Created once the negotiator's registers are in the map. */
#define RECORD_I2C_NEGOTIATOR "i2c_negotiator"

#ifdef __cplusplus
extern "C" {
#endif

bool i2c_negotiator_input_sw_button_event(SwInputKey key, bool pressed, void* context);

#ifdef __cplusplus
}
#endif
