#pragma once
#include <furi.h>

#define RECORD_LEDS "led"
typedef struct Led Led;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} LedColor;

#define LED_COLOR_RED        (LedColor){255, 0, 0}
#define LED_COLOR_GREEN      (LedColor){0, 255, 0}
#define LED_COLOR_BLUE       (LedColor){0, 0, 255}
#define LED_COLOR_YELLOW     (LedColor){255, 255, 0}
#define LED_COLOR_ORANGE     (LedColor){255, 165, 0}
#define LED_COLOR_LIGHT_BLUE (LedColor){0x12, 0xCD, 0xD4}
#define LED_COLOR_WHITE      (LedColor){255, 255, 255}
#define LED_COLOR_BLACK      (LedColor){0, 0, 0}

typedef enum {
    // line 1
    LedTypeNet,
    LedTypeWiFi,
    LedTypeEth2,
    LedTypeEth1,

    // line 2
    LedTypePower,
    LedTypeBatteryOutline, // 2 leds
    LedTypeBatteryWatt1 = LedTypeBatteryOutline + 2,
    LedTypeBatteryWatt2,
    LedTypeBatteryWatt3,
    LedTypeBatteryWatt4,

    // line 3
    LedTypeUsbCharging,
    LedTypeUsbWatt1,
    LedTypeUsbWatt2,
    LedTypeUsbWatt3,
    LedTypeUsbWatt4,
    LedTypeBatteryCenter,

    // special types
    LedTypeLine1Off,
    LedTypeLine2Off,
    LedTypeLine3Off,
    LedTypeLineAllOff,
} LedType;

typedef struct {
    const LedType type;
    const LedColor color;
} LedItem;

typedef struct {
    const LedItem* items;
    const size_t count;
} LedBatch;

#ifdef __cplusplus
extern "C" {
#endif

void led_set_color_single(Led* instance, LedType type, LedColor color);
void led_set_color_batch(Led* instance, const LedBatch* items);

#ifdef __cplusplus
}
#endif
