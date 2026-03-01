#pragma once

#include <led/led.h>

void led_set_color_batch_simple(const LedBatch* items);

extern const LedBatch led_batch_all_off;
extern const LedBatch led_batch_power_red;
extern const LedBatch led_batch_all_on;
extern const LedBatch led_batch_all_white;
