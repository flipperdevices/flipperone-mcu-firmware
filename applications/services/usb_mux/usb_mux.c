#include "usb_mux.h"

#include <drivers/hd3ss3220/hd3ss3220.h>
#include <furi_hal_i2c_config.h>
#include <furi_hal_resources.h>
#include <api_lock.h>

#define TAG "UsbMux"

#define USB_MUX_MAX_MESSAGES (8)

struct UsbMux {
    FuriEventLoop* event_loop;
    Hd3ss3220* usb_mux_header;
    FuriMessageQueue* message_queue;
    FuriEventLoopTimer* timer;
    UsbMuxDevice device;
};

typedef enum {
    UsbMuxMessageTypePlayEffect,
    UsbMuxMessageTypeStart,
    UsbMuxMessageTypeStop,
} UsbMuxMessageType;

typedef struct {
    UsbMuxMessageType type;
    FuriApiLock lock;
    bool* result;
    // union {
    //     struct {
    //         uint32_t test_value;
    //     } test;
    // } as;
} UsbMuxMessage;

static void usb_mux_message_queue_callback(FuriEventLoopObject* object, void* context) {
    furi_assert(context);
    UsbMux* instance = (UsbMux*)context;
    furi_assert(object == instance->message_queue);

    UsbMuxMessage msg;
    furi_check(furi_message_queue_get(instance->message_queue, &msg, 0) == FuriStatusOk);

    bool result = false;

    switch(msg.type) {
    default:
        furi_crash("Invalid message type");
        break;
    }

    if(msg.result) {
        *msg.result = result;
    }

    if(msg.lock) {
        api_lock_unlock(msg.lock);
    }
}

// static void usb_mux_send_message(UsbMux* instance, const UsbMuxMessage* message) {
//     furi_check(furi_message_queue_put(instance->message_queue, message, FuriWaitForever) == FuriStatusOk);

//     if(message->lock) {
//         api_lock_wait_unlock_and_free(message->lock);
//     }
// }

static UsbMux* usb_mux_alloc(void) {
    UsbMux* instance = (UsbMux*)malloc(sizeof(UsbMux));
    instance->event_loop = furi_event_loop_alloc();
    instance->message_queue = furi_message_queue_alloc(USB_MUX_MAX_MESSAGES, sizeof(UsbMuxMessage));

    instance->usb_mux_header = hd3ss3220_init(&furi_hal_i2c_handle_control, HD3SS3220_ADDRESS, NULL);
    if(instance->usb_mux_header) {
        instance->device |= UsbMuxDeviceHd3ss3220;
    } else {
        FURI_LOG_E(TAG, "Failed to initialize HD3SS3220");
    }

    if(instance->usb_mux_header) {
        // TODO: add interrupt support for hd3ss3220
    }

    furi_event_loop_subscribe_message_queue(instance->event_loop, instance->message_queue, FuriEventLoopEventIn, usb_mux_message_queue_callback, instance);
    furi_record_create(RECORD_USBMUX, instance);

    return instance;
}

bool usb_mux_is_device_initialized(UsbMux* instance, UsbMuxDevice* device) {
    furi_check(instance);
    bool initialized = (instance->device & UsbMuxDeviceHd3ss3220) == UsbMuxDeviceHd3ss3220;

    if(device) {
        *device = instance->device;
    }
    if(!initialized) {
        FURI_LOG_E(TAG, "UsbMux device not initialized");
    }
    return initialized;
}

int32_t usb_mux_srv(void* p) {
    UNUSED(p);

    UsbMux* instance = usb_mux_alloc();
    furi_event_loop_run(instance->event_loop);

    return 0;
}
