#pragma once
#include <furi.h>

#include <drivers/drv2605l/drv2605l_effect.h>

#define RECORD_HAPTIC "haptic"
typedef struct Haptic Haptic;

typedef enum {
    HapticModeCpu,
    HapticModeMpu,

    HapticModeCount,
} HapticMode;

#ifdef __cplusplus
extern "C" {
#endif

void haptic_notification(Haptic* instance, Drv2605lEffect effect_index);

#ifdef __cplusplus
}
#endif
