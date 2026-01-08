#include "haptic.h"

#include <drivers/drv2605l/drv2605l.h>
#include <furi_hal_i2c_config.h>
#include <furi_hal_resources.h>
#include <api_lock.h>

#define TAG "Haptic"

#define HAPTIC_MAX_MESSAGES   (8)
#define HAPTIC_TIMEOUT_OFF_MS (1000)

struct Haptic {
    FuriEventLoop* event_loop;
    Drv2605l* haptic_header;
    FuriMessageQueue* message_queue;
    FuriEventLoopTimer* timer;
};

typedef enum {
    HapticMessageTypeEffectPlay,
} HapticMessageType;

typedef struct {
    HapticMessageType type;
    FuriApiLock lock;
    bool* result;
    union {
        Drv2605lEffect effect_index;
    };
} HapticMessage;

void haptic_event_isr(void* context) {
    furi_assert(context);
    Haptic* instance = (Haptic*)context;
}

static FURI_ALWAYS_INLINE void haptic_start_off_timer(Haptic* instance) {
    furi_assert(instance);
    drv2605l_enable(instance->haptic_header);
    furi_event_loop_timer_start(instance->timer, HAPTIC_TIMEOUT_OFF_MS);
}

static void haptic_timer_callback(void* context) {
    furi_assert(context);
    Haptic* instance = (Haptic*)context;

    drv2605l_disable(instance->haptic_header);
}

static void haptic_message_queue_callback(FuriEventLoopObject* object, void* context) {
    furi_assert(context);
    Haptic* instance = context;
    furi_assert(object == instance->message_queue);

    HapticMessage msg;
    furi_check(furi_message_queue_get(instance->message_queue, &msg, 0) == FuriStatusOk);

    bool result = false;

    switch(msg.type) {
    case HapticMessageTypeEffectPlay:
        haptic_start_off_timer(instance);
        drv2605l_trigger_set_effect_and_play(instance->haptic_header, msg.effect_index);
        result = true;
        break;
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

static void haptic_send_message(Haptic* instance, const HapticMessage* message) {
    furi_check(furi_message_queue_put(instance->message_queue, message, FuriWaitForever) == FuriStatusOk);

    if(message->lock) {
        api_lock_wait_unlock_and_free(message->lock);
    }
}

static Haptic* haptic_alloc(void) {
    Haptic* instance = (Haptic*)malloc(sizeof(Haptic));
    instance->event_loop = furi_event_loop_alloc();
    instance->message_queue = furi_message_queue_alloc(HAPTIC_MAX_MESSAGES, sizeof(HapticMessage));

    instance->haptic_header = drv2605l_init(&furi_hal_i2c_handle_internal, &gpio_haptic_en, &gpio_haptic_pwm, DRV2605L_ADDRESS);

    furi_event_loop_subscribe_message_queue(instance->event_loop, instance->message_queue, FuriEventLoopEventIn, haptic_message_queue_callback, instance);

    instance->timer = furi_event_loop_timer_alloc(instance->event_loop, haptic_timer_callback, FuriEventLoopTimerTypeOnce, instance);

    furi_record_create(RECORD_HAPTIC, instance);

    return instance;
}

int32_t haptic_srv(void* p) {
    UNUSED(p);

    Haptic* instance = haptic_alloc();
    furi_event_loop_run(instance->event_loop);

    return 0;
}

void haptic_notification(Haptic* instance, Drv2605lEffect effect_index) {
    furi_check(instance);
    furi_check(effect_index < Drv2605lEffectCountMax);

    const HapticMessage msg = {
        .type = HapticMessageTypeEffectPlay,
        .effect_index = effect_index,
    };

    haptic_send_message(instance, &msg);
}
