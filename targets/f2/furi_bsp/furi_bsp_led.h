#pragma once

#include "furi_bsp_led_type.h"
#include <stdint.h>

typedef struct FuriBspLed FuriBspLed;

#define LED_LINE_1_LEDS_COUNT (4U)
#define LED_LINE_2_LEDS_COUNT (6U)
#define LED_LINE_3_LEDS_COUNT (6U)
#define LED_TOTAL_LEDS_COUNT  (LED_LINE_1_LEDS_COUNT + 1U + LED_LINE_2_LEDS_COUNT + LED_LINE_3_LEDS_COUNT ) // +1 for FuriBspLedTypePower

#ifdef __cplusplus
extern "C" {
#endif

FuriBspLed* furi_bsp_led_alloc(void);
void furi_bsp_led_free(FuriBspLed* instance);
void furi_bsp_led_set(FuriBspLed* instance, FuriBspLedType type, uint8_t r, uint8_t g, uint8_t b);
void furi_bsp_led_all_off(FuriBspLed* instance);
void furi_bsp_led_update(FuriBspLed* instance);

#ifdef __cplusplus
}
#endif
