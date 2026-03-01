#include "led_batch.h"

// all leds off

const LedItem led_batch_all_off_items[] = {
    {.type = LedTypeLineAllOff},
};

const LedBatch led_batch_all_off = {
    .items = led_batch_all_off_items,
    .count = COUNT_OF(led_batch_all_off_items),
};

// power red

const LedItem led_batch_power_red_items[] = {
    {.type = LedTypePower, .color = LED_COLOR_RED},
};

const LedBatch led_batch_power_red = {
    .items = led_batch_power_red_items,
    .count = COUNT_OF(led_batch_power_red_items),
};

// all leds on

const LedItem led_batch_all_on_items[] = {
    {.type = LedTypeNet, .color = LED_COLOR_BLUE},
    {.type = LedTypeWiFi, .color = LED_COLOR_BLUE},
    {.type = LedTypeEth2, .color = LED_COLOR_BLUE},
    {.type = LedTypeEth1, .color = LED_COLOR_BLUE},
    {.type = LedTypePower, .color = LED_COLOR_GREEN},
    {.type = LedTypeBatteryOutline, .color = LED_COLOR_GREEN},
    {.type = LedTypeBatteryWatt1, .color = LED_COLOR_RED},
    {.type = LedTypeBatteryWatt2, .color = LED_COLOR_RED},
    {.type = LedTypeBatteryWatt3, .color = LED_COLOR_YELLOW},
    {.type = LedTypeBatteryWatt4, .color = LED_COLOR_GREEN},
    {.type = LedTypeUsbCharging, .color = LED_COLOR_RED},
    {.type = LedTypeUsbWatt1, .color = LED_COLOR_RED},
    {.type = LedTypeUsbWatt2, .color = LED_COLOR_RED},
    {.type = LedTypeUsbWatt3, .color = LED_COLOR_YELLOW},
    {.type = LedTypeUsbWatt4, .color = LED_COLOR_GREEN},
    {.type = LedTypeBatteryCenter, .color = LED_COLOR_GREEN},
};

const LedBatch led_batch_all_on = {
    .items = led_batch_all_on_items,
    .count = COUNT_OF(led_batch_all_on_items),
};

// all leds white

const LedItem led_batch_all_white_items[] = {
    {.type = LedTypeNet, .color = LED_COLOR_WHITE},
    {.type = LedTypeWiFi, .color = LED_COLOR_WHITE},
    {.type = LedTypeEth2, .color = LED_COLOR_WHITE},
    {.type = LedTypeEth1, .color = LED_COLOR_WHITE},
    {.type = LedTypePower, .color = LED_COLOR_WHITE},
    {.type = LedTypeBatteryOutline, .color = LED_COLOR_WHITE},
    {.type = LedTypeBatteryWatt1, .color = LED_COLOR_WHITE},
    {.type = LedTypeBatteryWatt2, .color = LED_COLOR_WHITE},
    {.type = LedTypeBatteryWatt3, .color = LED_COLOR_WHITE},
    {.type = LedTypeBatteryWatt4, .color = LED_COLOR_WHITE},
    {.type = LedTypeUsbCharging, .color = LED_COLOR_WHITE},
    {.type = LedTypeUsbWatt1, .color = LED_COLOR_WHITE},
    {.type = LedTypeUsbWatt2, .color = LED_COLOR_WHITE},
    {.type = LedTypeUsbWatt3, .color = LED_COLOR_WHITE},
    {.type = LedTypeUsbWatt4, .color = LED_COLOR_WHITE},
    {.type = LedTypeBatteryCenter, .color = LED_COLOR_WHITE},
};

const LedBatch led_batch_all_white = {
    .items = led_batch_all_white_items,
    .count = COUNT_OF(led_batch_all_white_items),
};

// functions

void led_set_color_batch_simple(const LedBatch* items) {
    Led* led = furi_record_open(RECORD_LEDS);
    led_set_color_batch(led, items);
    furi_record_close(RECORD_LEDS);
}
