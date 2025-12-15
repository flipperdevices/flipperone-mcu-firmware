#include "input_touch.h"

#include <furi_hal_i2c_config.h>
#include <furi.h>
#include <drivers/iqs7211e/iqs7211e.h>

#define TAG "InputTouch"

// #define INPUT_TOUCH_DEBUG_ENABLE

#ifdef INPUT_TOUCH_DEBUG_ENABLE
#define INPUT_TOUCH_DEBUG(...) FURI_LOG_I(TAG, __VA_ARGS__)
#else
#define INPUT_TOUCH_DEBUG(...)
#endif

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
            INPUT_TOUCH_DEBUG("Single tap detected");
            break;
        case Iqs7211eEventDoubleTap:
            INPUT_TOUCH_DEBUG("Double tap detected");
            break;
        case Iqs7211eEventTripleTap:
            INPUT_TOUCH_DEBUG("Triple tap detected");
            break;
        case Iqs7211eEventPressAndHold:
            INPUT_TOUCH_DEBUG("Press and hold detected");
            break;
        case Iqs7211eEventPalmGesture:
            INPUT_TOUCH_DEBUG("Palm gesture detected");
            break;
        case Iqs7211eEventSwipeXPositive:
            INPUT_TOUCH_DEBUG("Swipe X Positive detected");
            break;
        case Iqs7211eEventSwipeXNegative:
            INPUT_TOUCH_DEBUG("Swipe X Negative detected");
            break;
        case Iqs7211eEventSwipeYPositive:
            INPUT_TOUCH_DEBUG("Swipe Y Positive detected");
            break;
        case Iqs7211eEventSwipeYNegative:
            INPUT_TOUCH_DEBUG("Swipe Y Negative detected");
            break;
        case Iqs7211eEventHoldXPositive:
            INPUT_TOUCH_DEBUG("Hold X Positive detected");
            break;
        case Iqs7211eEventHoldXNegative:
            INPUT_TOUCH_DEBUG("Hold X Negative detected");
            break;
        case Iqs7211eEventHoldYPositive:
            INPUT_TOUCH_DEBUG("Hold Y Positive detected");
            break;
        case Iqs7211eEventHoldYNegative:
            INPUT_TOUCH_DEBUG("Hold Y Negative detected");
            break;
        default:
            // FURI_LOG_E(TAG, "Unknown event detected: %04X", event);
            break;
        }
    }

    Iqs7211eChargingMode charging_mode = iqs7211e_get_charging_mode(instance->iqs7211e);
    if(charging_mode) {
        switch(charging_mode) {
        case Iqs7211eChargingModeActive:
            INPUT_TOUCH_DEBUG("Charging Mode: Active");
            break;
        case Iqs7211eChargingModeIdleTouch:
            INPUT_TOUCH_DEBUG("Charging Mode: Idle Touch");
            break;
        case Iqs7211eChargingModeIdle:
            INPUT_TOUCH_DEBUG("Charging Mode: Idle");
            break;
        case Iqs7211eChargingModeLP1:
            INPUT_TOUCH_DEBUG("Charging Mode: Low Power 1");
            break;
        case Iqs7211eChargingModeLP2:
            INPUT_TOUCH_DEBUG("Charging Mode: Low Power 2");
            break;
        default:
            FURI_LOG_E(TAG, "Unknown Charging Mode: %03b", charging_mode);
            break;
        }
    }

    if(iqs7211e_get_fingers_num(instance->iqs7211e)) {
        INPUT_TOUCH_DEBUG("X=%d, Y=%d;", iqs7211e_get_abs_x_fingers_num(instance->iqs7211e, 1), iqs7211e_get_abs_y_fingers_num(instance->iqs7211e, 1));
        InputTouchEvent event;
        event.type = InputTouchTypeMove;
        event.x = iqs7211e_get_abs_x_fingers_num(instance->iqs7211e, 1);
        event.y = iqs7211e_get_abs_y_fingers_num(instance->iqs7211e, 1);
        furi_pubsub_publish(instance->event_pubsub, &event);
    }
}

int32_t input_touch_srv(void* p) {
    UNUSED(p);

    InputTouch* instance = (InputTouch*)malloc(sizeof(InputTouch));

    instance->thread_id = furi_thread_get_current_id();
    instance->event_pubsub = furi_pubsub_alloc();
    instance->iqs7211e = iqs7211e_init(&furi_hal_i2c_handle_external, &gpio_touchpad_rdy, IQS7211E_ADDRESS);

    if(!instance->iqs7211e) {
        FURI_LOG_E(TAG, "Failed to initialize IQS7211E touchpad");
        while(1) {
            furi_delay_ms(1000);
        }
    }

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
