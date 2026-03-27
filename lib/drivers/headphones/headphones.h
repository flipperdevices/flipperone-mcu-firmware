#pragma once
#include <furi_hal_gpio.h>

typedef enum {
    HeadphonesStatusConnected = (1 << 0),
    HeadphonesStatusDisconnected = (1 << 1),
    HeadphonesStatusMicrophoneConnected = (1 << 2),
    HeadphonesStatusKeyPressedA = (1 << 3),
    HeadphonesStatusKeyPressedB = (1 << 4),
    HeadphonesStatusKeyPressedC = (1 << 5),
    HeadphonesStatusKeyPressedD = (1 << 6),
    HeadphonesStatusUnknown = 0xFFFF,
} HeadphonesStatus;

typedef void (*HeadphonesCallbackInput)(void* context, HeadphonesStatus hp_status);

#define HEADPHONES_STATUS_KEY_PRESSED_MASK \
    (HeadphonesStatusKeyPressedA | HeadphonesStatusKeyPressedB | HeadphonesStatusKeyPressedC | HeadphonesStatusKeyPressedD)

#ifdef __cplusplus
extern "C" {
#endif

void headphones_init(const GpioPin* headphone_detect_pin, const GpioPin* headphone_key_pin, HeadphonesCallbackInput callback, void* callback_context);
void headphones_deinit(void);
void headphones_update(void);

#ifdef __cplusplus
}
#endif
