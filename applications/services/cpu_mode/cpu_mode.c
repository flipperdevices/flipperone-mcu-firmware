#include "cpu_mode.h"

#include <api_lock.h>

#define TAG "CpuMode"

#define CPU_MODE_MAX_MESSAGES (8)

typedef struct {
    CpuState cpu_state;
} CpuModeStatus;

struct CpuMode {
    FuriEventLoop* event_loop;
    FuriMessageQueue* message_queue;
    CpuModeStatus status;
};

typedef enum {
    CpuModeMessageTypeSetCpuMode,
    CpuModeMessageTypeGetCpuMode,
} CpuModeMessageType;

typedef struct {
    CpuModeMessageType type;
    FuriApiLock lock;
    bool* result;
    union {
        struct {
            CpuState* cpu_state;
        } cpu_state;
    } as;
} CpuModeMessage;

static void cpu_mode_message_queue_callback(FuriEventLoopObject* object, void* context) {
    furi_assert(context);
    CpuMode* instance = context;
    furi_assert(object == instance->message_queue);

    CpuModeMessage msg;
    furi_check(furi_message_queue_get(instance->message_queue, &msg, 0) == FuriStatusOk);

    bool result = false;

    switch(msg.type) {
    case CpuModeMessageTypeSetCpuMode:
        instance->status.cpu_state = *msg.as.cpu_state.cpu_state;
        FURI_LOG_I(TAG, "CPU mode: %d", instance->status.cpu_state);
        result = true;
        break;
    case CpuModeMessageTypeGetCpuMode:
        if(msg.as.cpu_state.cpu_state) {
            *msg.as.cpu_state.cpu_state = instance->status.cpu_state;
            result = true;
        }
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

static void cpu_mode_send_message(CpuMode* instance, const CpuModeMessage* message) {
    furi_check(furi_message_queue_put(instance->message_queue, message, FuriWaitForever) == FuriStatusOk);

    if(message->lock) {
        api_lock_wait_unlock_and_free(message->lock);
    }
}

static CpuMode* cpu_mode_alloc(void) {
    CpuMode* instance = (CpuMode*)malloc(sizeof(CpuMode));
    instance->event_loop = furi_event_loop_alloc();
    instance->message_queue = furi_message_queue_alloc(CPU_MODE_MAX_MESSAGES, sizeof(CpuModeMessage));

    furi_event_loop_subscribe_message_queue(instance->event_loop, instance->message_queue, FuriEventLoopEventIn, cpu_mode_message_queue_callback, instance);

    furi_record_create(RECORD_CPU_MODE, instance);

    return instance;
}

int32_t cpu_mode_srv(void* p) {
    UNUSED(p);

    CpuMode* instance = cpu_mode_alloc();
    furi_event_loop_run(instance->event_loop);

    return 0;
}

bool cpu_mode_set_cpu_mode(CpuMode* instance, CpuState cpu_state) {
    furi_check(instance);
    bool result = false;
    const CpuModeMessage msg = {
        .type = CpuModeMessageTypeSetCpuMode,
        .result = &result,
        .lock = api_lock_alloc_locked(),
        .as = {
            .cpu_state = {
                .cpu_state = &cpu_state,
            },
        },
    };

    cpu_mode_send_message(instance, &msg);
    return result;
}

bool cpu_mode_get_cpu_mode(CpuMode* instance, CpuState* cpu_state) {
    furi_check(instance);
    bool result = false;
    const CpuModeMessage msg = {
        .type = CpuModeMessageTypeGetCpuMode,
        .result = &result,
        .lock = api_lock_alloc_locked(),
        .as = {
            .cpu_state = {
                .cpu_state = cpu_state,
            },
        },
    };

    cpu_mode_send_message(instance, &msg);
    return result;
}