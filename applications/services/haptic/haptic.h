#pragma once
#include <furi.h>

#include <drivers/drv2605l/drv2605l_effect.h>

#define RECORD_HAPTIC "haptic"

typedef struct Haptic Haptic;

typedef enum {
   HapticDeviceOk = (1 << 0),
} HapticDevice;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Checks if the Haptic device is initialized.
 * 
 * @param instance The Haptic instance.
 * @param device Pointer to store the initialized device(s).
 * @return true  if initialization was successful, false otherwise.
 */
bool haptic_is_device_initialized(Haptic* instance, HapticDevice* device);

/**
 * @brief Plays a haptic effect on the device. The effect will be automatically stopped after time_ms has elapsed, or can be stopped early by calling haptic_stop.
 * 
 * @param instance The Haptic instance.
 * @param effect_index The index of the effect to play.
 * @param time_ms The duration to play the effect in milliseconds.
 * @return true if the effect was successfully played, false otherwise.
 */
bool haptic_play_effect(Haptic* instance, Drv2605lEffect effect_index, uint32_t time_ms);

/**
 * @brief Starts previously played effect.
 * 
 * @param instance The Haptic instance.
 * @return true if the effect was successfully started, false otherwise.
 */
bool haptic_start(Haptic* instance);

/**
 * @brief Stops currently playing effect.
 * 
 * @param instance The Haptic instance.
 * @return true if the effect was successfully stopped, false otherwise.
 */
bool haptic_stop(Haptic* instance);

#ifdef __cplusplus
}
#endif
