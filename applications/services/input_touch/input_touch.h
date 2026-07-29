#pragma once

#include <furi_hal_resources.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RECORD_INPUT_TOUCH_EVENTS "input_touch_events"
#define RECORD_INPUT_TOUCH        "input_touch"

#define TOUCHPAD_RESOLUTION_X        1024
#define TOUCHPAD_RESOLUTION_Y        768
#define TOUCHPAD_RESOLUTION_PRESSURE (1024 * 16)

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

/** Get human readable input type name
 * @param type - InputTouchType
 * @return string
 */
const char* input_touch_get_type_name(InputTouchType type);

#ifdef __cplusplus
}
#endif
