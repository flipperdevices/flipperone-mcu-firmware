#pragma once

#include <furi_hal_resources.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RECORD_INPUT_TOUCH_EVENTS "input_touch_events"

typedef enum {
    InputTouchTypeMove,
    InputTouchTypeMAX, /**< Special value for exceptional */
} InputTouchType;

/** Input Event, dispatches with FuriPubSub */
typedef struct {
    union {
        struct {
            uint32_t x;
            uint32_t y;
        };
    };
    InputTouchType type;
} InputTouchEvent;

#ifdef __cplusplus
}
#endif
