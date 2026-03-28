#pragma once
#include <furi_hal_gpio.h>

typedef enum {
    HeadphonesStatusNone = 0,
    HeadphonesStatusDisconnected = (1 << 0),
    HeadphonesStatusConnected = (1 << 1),
    HeadphonesStatusMicrophoneConnected = (1 << 2),
    HeadphonesStatusKeyPressedA = (1 << 3),
    HeadphonesStatusKeyPressedB = (1 << 4),
    HeadphonesStatusKeyPressedC = (1 << 5),
    HeadphonesStatusKeyPressedD = (1 << 6),
    HeadphonesStatusUnknown = 0xFFFF,
} HeadphonesStatus;

typedef void (*HeadphonesCallback)(void* context, bool connected);

#define HEADPHONES_STATUS_KEY_PRESSED_MASK \
    (HeadphonesStatusKeyPressedA | HeadphonesStatusKeyPressedB | HeadphonesStatusKeyPressedC | HeadphonesStatusKeyPressedD)

#ifdef __cplusplus
extern "C" {
#endif

void headphones_init(const GpioPin* headphone_detect_pin, const GpioPin* headphone_key_pin, HeadphonesCallback callback, void* callback_context);
void headphones_deinit(void);
bool headphones_update(HeadphonesStatus* status);

#ifdef __cplusplus
}
#endif
