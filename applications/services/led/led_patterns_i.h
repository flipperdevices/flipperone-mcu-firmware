#pragma once

#include "led.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    LedColor (*callback)(LedColor base_color, uint8_t* phase);
    uint32_t period_ms;
} LedPatternPreset;

extern const LedPatternPreset led_pattern_presets[LedPatternMax];

#ifdef __cplusplus
}
#endif
