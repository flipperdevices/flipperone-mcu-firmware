#pragma once

#include <furi_hal_i2c_types.h>
#include <furi_hal_gpio.h>
#include <drivers/drv2605l/drv2605l_effect.h>

#define DRV2605L_ADDRESS (0x5Au)

typedef struct Drv2605l Drv2605l;

typedef enum {
    Drv2605lModeTriggerGo = 0b000,
    Drv2605lModeTriggerExternalEdge = 0b001,
    Drv2605lModeTriggerExternalLevel = 0b010,
    Drv2605lModeTriggerPWM = 0b011,
    Drv2605lModeTriggerAudioVibe = 0b100,
    Drv2605lModeTriggerRealtime = 0b101,
} Drv2605lModeTrigger;

#ifdef __cplusplus
extern "C" {
#endif

Drv2605l* drv2605l_init(const FuriHalI2cBusHandle* i2c_handle, const GpioPin* pin_en, const GpioPin* pin_trigger, uint8_t address);
void drv2605l_deinit(Drv2605l* instance);
void drv2605l_enable(Drv2605l* instance);
void drv2605l_disable(Drv2605l* instance);
void drv2605l_trigger_set_effect(Drv2605l* instance, Drv2605lModeTrigger trigger_mode, Drv2605lEffect effect_index);
void drv2605l_trigger_go(Drv2605l* instance);
void drv2605l_trigger_set_effect_and_play(Drv2605l* instance, Drv2605lEffect effect_index);
void drv2605l_test_all_effects(Drv2605l* instance);

#ifdef __cplusplus
}
#endif
