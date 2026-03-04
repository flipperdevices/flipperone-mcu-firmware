#include "haptic.h"

#include <drivers/drv2605l/drv2605l.h>
#include <furi_hal_i2c_config.h>
#include <furi_hal_resources.h>
#include <api_lock.h>

#define TAG "Haptic"

#define HAPTIC_MAX_MESSAGES   (8)
#define HAPTIC_TIMEOUT_OFF_MS (3000)

struct Haptic {
    FuriEventLoop* event_loop;
    Drv2605l* haptic_header;
    FuriMessageQueue* message_queue;
    FuriEventLoopTimer* timer;
};

typedef enum {
    HapticMessageTypePlayEffect,
    HapticMessageTypeStart,
    HapticMessageTypeStop,
} HapticMessageType;

typedef struct {
    HapticMessageType type;
    FuriApiLock lock;
    bool* result;
    union {
        struct {
            Drv2605lEffect effect_index;
            uint32_t time_ms;
        } play_effect;
    } as;
} HapticMessage;

static FURI_ALWAYS_INLINE void haptic_start_off_timer(Haptic* instance, uint32_t play_time_ms) {
    furi_assert(instance);
    drv2605l_enable(instance->haptic_header);
    furi_event_loop_timer_start(instance->timer, play_time_ms);
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
    case HapticMessageTypePlayEffect:
        haptic_start_off_timer(instance, msg.as.play_effect.time_ms);
        drv2605l_trigger_set_effect_and_play(instance->haptic_header, msg.as.play_effect.effect_index);
        result = true;
        break;
    case HapticMessageTypeStart:
        haptic_start_off_timer(instance, HAPTIC_TIMEOUT_OFF_MS);
        drv2605l_trigger_go(instance->haptic_header, true);
        result = true;
        break;
    case HapticMessageTypeStop:
        haptic_start_off_timer(instance, HAPTIC_TIMEOUT_OFF_MS);
        drv2605l_trigger_go(instance->haptic_header, false);
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

    instance->haptic_header = drv2605l_init(&furi_hal_i2c_handle_control, &gpio_haptic_en, &gpio_haptic_pwm, DRV2605L_ADDRESS);

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

void haptic_play_effect(Haptic* instance, Drv2605lEffect effect_index, uint32_t time_ms) {
    furi_check(instance);
    furi_check(effect_index < Drv2605lEffectCountMax);

    const HapticMessage msg = {
        .type = HapticMessageTypePlayEffect,
        .as.play_effect =
            {
                .effect_index = effect_index,
                .time_ms = time_ms <= 1 ? HAPTIC_TIMEOUT_OFF_MS : time_ms,
            },
    };

    haptic_send_message(instance, &msg);
}

void haptic_start(Haptic* instance) {
    furi_check(instance);

    const HapticMessage msg = {
        .type = HapticMessageTypeStart,
    };

    haptic_send_message(instance, &msg);
}

void haptic_stop(Haptic* instance) {
    furi_check(instance);

    const HapticMessage msg = {
        .type = HapticMessageTypeStop,
    };

    haptic_send_message(instance, &msg);
}