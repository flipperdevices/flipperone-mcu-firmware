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

void haptic_notification_effect(Haptic* instance, Drv2605lEffect effect_index);
void haptic_notification_play(Haptic* instance, bool notify_play);

#ifdef __cplusplus
}
#endif
