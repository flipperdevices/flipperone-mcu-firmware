#pragma once

#include <furi.h>

// The record's value is a FuriPubSub* directly (like RECORD_HEADPHONES /
// RECORD_INPUT_TOUCH_EVENTS) - subscribe with:
//   furi_pubsub_subscribe(furi_record_open(RECORD_TOUCHPAD_TEST), callback, context);
#define RECORD_TOUCHPAD_TEST "touchpad_test"

typedef enum {
    TouchpadTestStatusStarted, // test view has been created and is ready
    TouchpadTestStatusCleared, // the "Clear" button/key was pressed
    TouchpadTestStatusDiamondFilled, // a new diamond was filled; see diamond_x/y
    TouchpadTestStatusAllFilled, // every diamond in the oval is now filled
} TouchpadTestStatus;

typedef struct {
    TouchpadTestStatus tp_status;
    int32_t diamond_x; // valid only for TouchpadTestStatusDiamondFilled
    int32_t diamond_y; // valid only for TouchpadTestStatusDiamondFilled
} TouchpadTestEvent;

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif
