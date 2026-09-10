#include "led_batch.h"

// all leds off

const LedItem led_batch_all_off_items[] = {
    {.type = FuriBspLedTypeAllOff},
};

const LedBatch led_batch_all_off = {
    .items = led_batch_all_off_items,
    .count = COUNT_OF(led_batch_all_off_items),
};

// power red

const LedItem led_batch_power_red_items[] = {
    {.type = FuriBspLedTypePower, .color = LED_COLOR_RED},
};

const LedBatch led_batch_power_red = {
    .items = led_batch_power_red_items,
    .count = COUNT_OF(led_batch_power_red_items),
};

// all leds on

const LedItem led_batch_all_on_items[] = {
    {.type = FuriBspLedTypeNet, .color = LED_COLOR_BLUE},
    {.type = FuriBspLedTypeWiFi, .color = LED_COLOR_BLUE},
    {.type = FuriBspLedTypeEth2, .color = LED_COLOR_BLUE},
    {.type = FuriBspLedTypeEth1, .color = LED_COLOR_BLUE},
    {.type = FuriBspLedTypePower, .color = LED_COLOR_GREEN},
    {.type = FuriBspLedTypeBatteryOutline, .color = LED_COLOR_GREEN},
    {.type = FuriBspLedTypeBatteryWatt1, .color = LED_COLOR_RED},
    {.type = FuriBspLedTypeBatteryWatt2, .color = LED_COLOR_RED},
    {.type = FuriBspLedTypeBatteryWatt3, .color = LED_COLOR_YELLOW},
    {.type = FuriBspLedTypeBatteryWatt4, .color = LED_COLOR_GREEN},
    {.type = FuriBspLedTypeUsbCharging, .color = LED_COLOR_RED},
    {.type = FuriBspLedTypeUsbWatt1, .color = LED_COLOR_RED},
    {.type = FuriBspLedTypeUsbWatt2, .color = LED_COLOR_RED},
    {.type = FuriBspLedTypeUsbWatt3, .color = LED_COLOR_YELLOW},
    {.type = FuriBspLedTypeUsbWatt4, .color = LED_COLOR_GREEN},
    {.type = FuriBspLedTypeBatteryCenter, .color = LED_COLOR_GREEN},
};

const LedBatch led_batch_all_on = {
    .items = led_batch_all_on_items,
    .count = COUNT_OF(led_batch_all_on_items),
};

// all leds white

const LedItem led_batch_all_white_items[] = {
    {.type = FuriBspLedTypeNet, .color = LED_COLOR_WHITE},
    {.type = FuriBspLedTypeWiFi, .color = LED_COLOR_WHITE},
    {.type = FuriBspLedTypeEth2, .color = LED_COLOR_WHITE},
    {.type = FuriBspLedTypeEth1, .color = LED_COLOR_WHITE},
    {.type = FuriBspLedTypePower, .color = LED_COLOR_WHITE},
    {.type = FuriBspLedTypeBatteryOutline, .color = LED_COLOR_WHITE},
    {.type = FuriBspLedTypeBatteryWatt1, .color = LED_COLOR_WHITE},
    {.type = FuriBspLedTypeBatteryWatt2, .color = LED_COLOR_WHITE},
    {.type = FuriBspLedTypeBatteryWatt3, .color = LED_COLOR_WHITE},
    {.type = FuriBspLedTypeBatteryWatt4, .color = LED_COLOR_WHITE},
    {.type = FuriBspLedTypeUsbCharging, .color = LED_COLOR_WHITE},
    {.type = FuriBspLedTypeUsbWatt1, .color = LED_COLOR_WHITE},
    {.type = FuriBspLedTypeUsbWatt2, .color = LED_COLOR_WHITE},
    {.type = FuriBspLedTypeUsbWatt3, .color = LED_COLOR_WHITE},
    {.type = FuriBspLedTypeUsbWatt4, .color = LED_COLOR_WHITE},
    {.type = FuriBspLedTypeBatteryCenter, .color = LED_COLOR_WHITE},
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
