#pragma once
#include <furi_hal_gpio.h>
#include "headphones_i.h"

typedef void (*HeadphonesCallback)(void* context, bool connected);

#ifdef __cplusplus
extern "C" {
#endif

void headphones_init(const GpioPin* headphone_detect_pin, const GpioPin* headphone_key_pin, HeadphonesCallback callback, void* callback_context);
void headphones_deinit(void);
bool headphones_update(HeadphonesStatus* status);

#ifdef __cplusplus
}
#endif
