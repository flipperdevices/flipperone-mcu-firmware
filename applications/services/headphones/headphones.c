#include "headphones.h"

#include <drivers/headphones/headphones.h>
#include <furi_hal_resources.h>

#define TAG "HeadphonesSrv"

#define HEADPHONES_TIMEOUT_UPDATE_MS             (100)
#define HEADPHONES_CHECK_CONNECT_DEBOUNCE_COUTER 5 // Timeout = HEADPHONES_CHECK_CONNECT_DEBOUNCE_COUTER * HEADPHONES_TIMEOUT_UPDATE_MS

#define HEADPHONES_SRV_DEBUG_ENABLE

#ifdef HEADPHONES_SRV_DEBUG_ENABLE
#define HEADPHONES_SRV_DEBUG(...) FURI_LOG_I(__VA_ARGS__)
#else
#define HEADPHONES_SRV_DEBUG(...)
#endif

struct Headphones {
    FuriEventLoop* event_loop;
    FuriEventLoopTimer* timer;
    FuriPubSub* event_pubsub;
    uint32_t debounce_counter;
};

typedef enum {
    HeadphonesEventTypeIsrConnected = (1 << 0),
    HeadphonesEventTypeIsrDisconnected = (1 << 1),
    HeadphonesEventTypeIsrTimer = (1 << 2),
} HeadphonesEventType;

static void __isr __not_in_flash_func(headphones_connected_callback)(void* context, bool connected) {
    Headphones* instance = (Headphones*)context;
    UNUSED(instance);
    if(connected) {
        furi_event_loop_set_custom_event(instance->event_loop, HeadphonesEventTypeIsrConnected);
    } else {
        furi_event_loop_set_custom_event(instance->event_loop, HeadphonesEventTypeIsrDisconnected);
    }
}

static void headphones_timer_callback(void* context) {
    furi_assert(context);
    Headphones* instance = (Headphones*)context;
    furi_event_loop_set_custom_event(instance->event_loop, HeadphonesEventTypeIsrTimer);
}

static void headphones_custom_event_callback(uint32_t events, void* context) {
    furi_assert(context);
    Headphones* instance = (Headphones*)context;
    HeadphonesStatus hp_status;

    if(events & HeadphonesEventTypeIsrConnected) {
        furi_event_loop_timer_start(instance->timer, HEADPHONES_TIMEOUT_UPDATE_MS);
        instance->debounce_counter = 0;
    }

    if(events & HeadphonesEventTypeIsrDisconnected) {
        instance->debounce_counter = 0;
        if(headphones_update(&hp_status)) {
            HEADPHONES_SRV_DEBUG(TAG, "Headphones status changed: %08b", hp_status);
            HeadphonesEvent event = {
                .hp_status = hp_status,
            };
            furi_pubsub_publish(instance->event_pubsub, &event);
        }
        furi_event_loop_timer_stop(instance->timer);
    }

    if(events & HeadphonesEventTypeIsrTimer) {
        if(instance->debounce_counter < HEADPHONES_CHECK_CONNECT_DEBOUNCE_COUTER) {
            instance->debounce_counter++;
            furi_event_loop_timer_start(instance->timer, HEADPHONES_TIMEOUT_UPDATE_MS);
        } else {
            if(headphones_update(&hp_status)) {
                HEADPHONES_SRV_DEBUG(TAG, "Headphones status changed: %08b", hp_status);
                HeadphonesEvent event = {
                    .hp_status = hp_status,
                };
                furi_pubsub_publish(instance->event_pubsub, &event);
            }
        }
    }
}

static Headphones* headphones_alloc(void) {
    Headphones* instance = (Headphones*)malloc(sizeof(Headphones));
    instance->event_loop = furi_event_loop_alloc();

    furi_event_loop_set_custom_event_callback(instance->event_loop, headphones_custom_event_callback, instance);
    instance->timer = furi_event_loop_timer_alloc(instance->event_loop, headphones_timer_callback, FuriEventLoopTimerTypePeriodic, instance);
    instance->event_pubsub = furi_pubsub_alloc();

    headphones_init(&gpio_audio_hp_detect, &gpio_audio_key, headphones_connected_callback, instance);

    furi_record_create(RECORD_HEADPHONES, instance->event_pubsub);

    return instance;
}

int32_t headphones_srv(void* p) {
    UNUSED(p);

    Headphones* instance = headphones_alloc();
    furi_event_loop_run(instance->event_loop);

    return 0;
}
