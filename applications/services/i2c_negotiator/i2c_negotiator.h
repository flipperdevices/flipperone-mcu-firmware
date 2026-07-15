#pragma once
#include <furi.h>


#ifdef __cplusplus
extern "C" {
#endif

bool i2c_negotiator_input_sw_button_event(SwInputKey key, bool pressed, void* context);

#ifdef __cplusplus
}
#endif
