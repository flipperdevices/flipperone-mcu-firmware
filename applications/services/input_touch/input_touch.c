#include "input_touch.h"

#include "core/check.h"
#include "core/log.h"
#include <complex.h>
#include <furi_hal_i2c_config.h>
#include <furi.h>
#include <drivers/iqs7211e/iqs7211e.h>
#include <stdint.h>

#define TAG "InputTouch"

// #define INPUT_DEBOUNCE_TICKS      4
// #define INPUT_DEBOUNCE_TICKS_HALF (INPUT_DEBOUNCE_TICKS / 2)
// #define INPUT_PRESS_TICKS         150
// #define INPUT_LONG_PRESS_COUNTS   2
#define INPUT_THREAD_FLAG_ISR 0x00000001

typedef struct {
    FuriPubSub* event_pubsub;
    FuriThreadId thread_id;
    Iqs7211e* iqs7211e;
} InputTouch;

void input_touch_isr(void* context) {
    furi_assert(context);
    InputTouch* instance = (InputTouch*)context;
    furi_thread_flags_set(instance->thread_id, INPUT_THREAD_FLAG_ISR);
}

void input_touch_event_isr(void* context) {
    furi_assert(context);
    InputTouch* instance = (InputTouch*)context;
    Iqs7211eEvent event = iqs7211e_get_event(instance->iqs7211e);
    if(event) {
        switch(event) {
        case Iqs7211eEventSingleTap:
            FURI_LOG_I(TAG, "Single tap detected");
            break;
        case Iqs7211eEventDoubleTap:
            FURI_LOG_I(TAG, "Double tap detected");
            break;
        case Iqs7211eEventTripleTap:
            FURI_LOG_I(TAG, "Triple tap detected");
            break;
        case Iqs7211eEventPressAndHold:
            FURI_LOG_I(TAG, "Press and hold detected");
            break;
        case Iqs7211eEventPalmGesture:
            FURI_LOG_I(TAG, "Palm gesture detected");
            break;
        case Iqs7211eEventSwipeXPositive:
            FURI_LOG_I(TAG, "Swipe X Positive detected");
            break;
        case Iqs7211eEventSwipeXNegative:
            FURI_LOG_I(TAG, "Swipe X Negative detected");
            break;
        case Iqs7211eEventSwipeYPositive:
            FURI_LOG_I(TAG, "Swipe Y Positive detected");
            break;
        case Iqs7211eEventSwipeYNegative:
            FURI_LOG_I(TAG, "Swipe Y Negative detected");
            break;
        case Iqs7211eEventHoldXPositive:
            FURI_LOG_I(TAG, "Hold X Positive detected");
            break;
        case Iqs7211eEventHoldXNegative:
            FURI_LOG_I(TAG, "Hold X Negative detected");
            break;
        case Iqs7211eEventHoldYPositive:
            FURI_LOG_I(TAG, "Hold Y Positive detected");
            break;
        case Iqs7211eEventHoldYNegative:
            FURI_LOG_I(TAG, "Hold Y Negative detected");
            break;
        default:
            FURI_LOG_W(TAG, "Unknown event detected: %04X", event);
            break;
        }
    }

    Iqs7211eChargingMode charging_mode = iqs7211e_get_charging_mode(instance->iqs7211e);
    if(charging_mode) {
        switch(charging_mode) {
        case Iqs7211eChargingModeActive:
            FURI_LOG_I(TAG, "Charging Mode: Active");
            break;
        case Iqs7211eChargingModeIdleTouch:
            FURI_LOG_I(TAG, "Charging Mode: Idle Touch");
            break;
        case Iqs7211eChargingModeIdle:
            FURI_LOG_I(TAG, "Charging Mode: Idle");
            break;
        case Iqs7211eChargingModeLP1:
            FURI_LOG_I(TAG, "Charging Mode: Low Power 1");
            break;
        case Iqs7211eChargingModeLP2:
            FURI_LOG_I(TAG, "Charging Mode: Low Power 2");
            break;
        default:
            FURI_LOG_W(TAG, "Unknown Charging Mode: %03b", charging_mode);
            break;
        }
    }

    if(iqs7211e_get_fingers_num(instance->iqs7211e)) {
        FURI_LOG_I(TAG, "X=%d, Y=%d;", iqs7211e_get_abs_x_fingers_num(instance->iqs7211e, 1), iqs7211e_get_abs_y_fingers_num(instance->iqs7211e, 1));
    }
}

int32_t input_touch_srv(void* p) {
    UNUSED(p);

    InputTouch* instance = (InputTouch*)malloc(sizeof(InputTouch));

    instance->thread_id = furi_thread_get_current_id();
    instance->event_pubsub = furi_pubsub_alloc();
    instance->iqs7211e = iqs7211e_init(&furi_hal_i2c_handle_internal, &gpio_touchpad_rdy, IQS7211E_ADDRESS);
    furi_record_create(RECORD_INPUT_TOUCH_EVENTS, instance->event_pubsub);
    iqs7211e_set_input_callback(instance->iqs7211e, input_touch_isr, input_touch_event_isr, instance);

#ifdef SRV_CLI
    CliRegistry* registry = furi_record_open(RECORD_CLI);
    cli_registry_add_command(registry, "input_touch", CliCommandFlagParallelSafe, input_touch_cli, instance->event_pubsub);
    furi_record_close(RECORD_CLI);
#endif

    while(1) {
        furi_thread_flags_wait(INPUT_THREAD_FLAG_ISR, FuriFlagWaitAny, FuriWaitForever);
        while(iqs7211e_get_ready(instance->iqs7211e)) {
            iqs7211e_run(instance->iqs7211e);
        }
    }

    return 0;
}
