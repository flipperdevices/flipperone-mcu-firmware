#include "input_touch.h"

#include <furi_hal_i2c_config.h>
#include <furi.h>
#include <drivers/iqs7211e/iqs7211e.h>

#define TAG "InputTouch"

#ifdef INPUT_TOUCH_DEBUG_ENABLE
#define INPUT_TOUCH_DEBUG(...) FURI_LOG_I(TAG, __VA_ARGS__)
#else
#define INPUT_TOUCH_DEBUG(...)
#endif

#define INPUT_THREAD_FLAG_ISR 0x00000001

struct InputTouch {
    FuriPubSub* event_pubsub;
    FuriThreadId thread_id;
    Iqs7211e* iqs7211e;
    InputTouchDevice device;
    bool touch;
};

static void __isr __not_in_flash_func(input_touch_isr)(void* context) {
    furi_assert(context);
    InputTouch* instance = (InputTouch*)context;
    furi_thread_flags_set(instance->thread_id, INPUT_THREAD_FLAG_ISR);
}

static void input_touch_send_event(InputTouch* instance, InputTouchType type) {
    InputTouchEvent event;
    event.type = type;
    event.x = iqs7211e_get_finger_abs_x(instance->iqs7211e, 1);
    event.y = iqs7211e_get_finger_abs_y(instance->iqs7211e, 1);
    event.pressure = iqs7211e_get_finger_touch_strength(instance->iqs7211e, 1);
    furi_pubsub_publish(instance->event_pubsub, &event);
}

static void input_touch_event_isr(void* context) {
    furi_assert(context);
    InputTouch* instance = (InputTouch*)context;

    // TODO: Sequenced touch events, to send TouchTypeEnd/TouchTypeStart in case of app switch
    {
        uint8_t finger_present = iqs7211e_get_fingers_num(instance->iqs7211e);

        if(instance->touch) {
            if(finger_present) {
                input_touch_send_event(instance, InputTouchTypeMove);
                INPUT_TOUCH_DEBUG("Touch Move");
            } else {
                input_touch_send_event(instance, InputTouchTypeEnd);
                INPUT_TOUCH_DEBUG("Touch End");
                instance->touch = false;
            }
        } else {
            if(finger_present) {
                input_touch_send_event(instance, InputTouchTypeStart);
                INPUT_TOUCH_DEBUG("Touch Start");
                instance->touch = true;
            }
        }
    }
}

int32_t input_touch_srv(void* p) {
    UNUSED(p);

    InputTouch* instance = (InputTouch*)malloc(sizeof(InputTouch));

    instance->thread_id = furi_thread_get_current_id();
    instance->event_pubsub = furi_pubsub_alloc();
    instance->iqs7211e = iqs7211e_init(&furi_hal_i2c_handle_control, &gpio_touchpad_rdy, IQS7211E_ADDRESS);

    furi_record_create(RECORD_INPUT_TOUCH_EVENTS, instance->event_pubsub);
    furi_record_create(RECORD_INPUT_TOUCH, instance);

    if(!instance->iqs7211e) {
        FURI_LOG_E(TAG, "Not initialized IQS7211E, input touch service cannot run");
        while(1) {
            furi_delay_ms(FuriWaitForever);
        }
    } else {
        instance->device |= InputTouchDeviceIqs7211e;
    }

    iqs7211e_set_input_callback(instance->iqs7211e, input_touch_isr, input_touch_event_isr, instance);

    while(1) {
        furi_thread_flags_wait(INPUT_THREAD_FLAG_ISR, FuriFlagWaitAny, FuriWaitForever);
        while(iqs7211e_get_ready(instance->iqs7211e)) {
            iqs7211e_run(instance->iqs7211e);
        }
    }

    return 0;
}

bool input_touch_is_device_initialized(InputTouch* instance, InputTouchDevice* device) {
    furi_check(instance);
    bool initialized = (instance->device & InputTouchDeviceIqs7211e) == InputTouchDeviceIqs7211e;

    if(device) {
        *device = instance->device;
    }
    if(!initialized) {
        FURI_LOG_E(TAG, "Input touch device not initialized");
    }
    return initialized;
}
