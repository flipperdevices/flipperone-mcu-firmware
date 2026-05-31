#pragma once

#include <furi_hal_resources.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RECORD_INPUT_TOUCH_EVENTS "input_touch_events"
#define RECORD_INPUT_TOUCH        "input_touch"

typedef struct InputTouch InputTouch;

typedef enum {
    InputTouchDeviceIqs7211e = (1 << 0),
} InputTouchDevice;

typedef enum {
    InputTouchTypeMove,
    InputTouchTypeStart,
    InputTouchTypeEnd,
    InputTouchTypeMAX, /**< Special value for exceptional */
} InputTouchType;

/** Input Event, dispatches with FuriPubSub */
typedef struct {
    union {
        struct {
            int32_t x;
            int32_t y;
            int32_t pressure;
        };
    };
    InputTouchType type;
} InputTouchEvent;

bool input_touch_is_device_initialized(InputTouch* instance, InputTouchDevice* device);

#ifdef __cplusplus
}
#endif
