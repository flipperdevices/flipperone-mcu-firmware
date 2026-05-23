#include "pd.h"

#include <furi.h>
#include <api_lock.h>
#include <furi_hal_i2c_config.h>
#include <furi_hal_resources.h>
#include <drivers/fusb302/fusb302.h>
#include <furi_bsp.h>

#define TAG "Pd"

#define PD_MAX_MESSAGES (8)

typedef enum {
    PdEventTypeIsr = (1 << 0),
    PdEventTypeAll = (PdEventTypeIsr),
} PdEventType;

struct Pd {
    FuriEventLoop* event_loop;
    FuriPubSub* event_pubsub;
    Fusb302* fusb302_header;
    PdMode mode;
    FuriMessageQueue* message_queue;
    PdDevice device;
};

typedef enum {
    PdMessageTypeSetMode,
    PdMessageTypeGetMode,
    PdMessageTypeResetConfig,
} PdMessageType;

typedef struct {
    PdMessageType type;
    FuriApiLock lock;
    bool* result;
    union {
        PdMode* get_mode;
        PdMode set_mode;
    };
} PdMessage;

static void __isr __not_in_flash_func(pd_event_isr)(void* context) {
    Pd* instance = (Pd*)context;
    furi_event_loop_set_custom_event(instance->event_loop, PdEventTypeIsr);
}

static void pd_message_queue_callback(FuriEventLoopObject* object, void* context) {
    furi_assert(context);
    Pd* instance = context;
    furi_assert(object == instance->message_queue);

    PdMessage msg;
    furi_check(furi_message_queue_get(instance->message_queue, &msg, 0) == FuriStatusOk);

    bool result = false;

    switch(msg.type) {
    case PdMessageTypeSetMode:
        instance->mode = msg.set_mode;
        break;
    case PdMessageTypeGetMode:
        *(msg.get_mode) = instance->mode;
        break;
    case PdMessageTypeResetConfig:
        result = fusb302_sw_reset(instance->fusb302_header) == Fusb302StatusOk;
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

static void pd_custom_event_callback(uint32_t events, void* context) {
    furi_assert(context);
    Pd* instance = (Pd*)context;

    if(events & PdEventTypeIsr) {
        if(instance->mode == PdModeOff) {
            return;
        }

        Fusb302ReadRoleResult role_result = fusb302_read_role(instance->fusb302_header);

        if(role_result == Fusb302ReadRoleResultToggleDone) {
            Fusb302PortState fusb302_port_state = Fusb302PortStateUndefined;
            Fusb302Status res = fusb302_get_port_state(instance->fusb302_header, &fusb302_port_state);
            if(res != Fusb302StatusOk) {
                FURI_LOG_E(TAG, "Failed to get port state");
                return;
            }

            PdPortState pd_state = PdPortStateUndefined;
            switch(fusb302_port_state) {
            case Fusb302PortStateToggling: pd_state = PdPortStateToggling; break;
            case Fusb302PortStateSourceCC1: pd_state = PdPortStateSourceCC1; break;
            case Fusb302PortStateSourceCC2: pd_state = PdPortStateSourceCC2; break;
            case Fusb302PortStateSinkCC1: pd_state = PdPortStateSinkCC1; break;
            case Fusb302PortStateSinkCC2: pd_state = PdPortStateSinkCC2; break;
            case Fusb302PortStateAudioAccessory: pd_state = PdPortStateAudioAccessory; break;
            case Fusb302PortStateOvercurrent: pd_state = PdPortStateUndefined; break; // not a TOGSS value, can never be returned by fusb302_get_port_state
            case Fusb302PortStateUndefined: pd_state = PdPortStateUndefined; break;
            }
            PdEvent event = {.port_state = pd_state};
            furi_pubsub_publish(instance->event_pubsub, &event);
        }
        else if (role_result == Fusb302ReadRoleResultOvercurrent) {
            PdPortState pd_state = PdPortStateOvercurrent;
            PdEvent event = {.port_state = pd_state};
            furi_pubsub_publish(instance->event_pubsub, &event);
        }
    }
}

static void pd_send_message(Pd* instance, const PdMessage* message) {
    furi_check(furi_message_queue_put(instance->message_queue, message, FuriWaitForever) == FuriStatusOk);

    if(message->lock) {
        api_lock_wait_unlock_and_free(message->lock);
    }
}

static Pd* pd_alloc(void) {
    Pd* instance = (Pd*)malloc(sizeof(Pd));
    instance->event_loop = furi_event_loop_alloc();
    instance->message_queue = furi_message_queue_alloc(PD_MAX_MESSAGES, sizeof(PdMessage));
    instance->fusb302_header = fusb302_init(&furi_hal_i2c_handle_main, FUSB302_ADDRESS, NULL);

    if(instance->fusb302_header) {
        instance->device |= PdDeviceFusb302;

        furi_bsp_expander_main_attach_fusb302_callback(pd_event_isr, instance);
        instance->mode = PdModeOff;

    } else {
        FURI_LOG_E(TAG, "Failed to initialize FUSB302");
    }

    furi_event_loop_subscribe_message_queue(instance->event_loop, instance->message_queue, FuriEventLoopEventIn, pd_message_queue_callback, instance);
    furi_event_loop_set_custom_event_callback(instance->event_loop, pd_custom_event_callback, instance);

    instance->event_pubsub = furi_pubsub_alloc();
    furi_record_create(RECORD_PD, instance);

    return instance;
}

bool pd_is_device_initialized(Pd* instance, PdDevice* device) {
    furi_check(instance);
    bool initialized = (instance->device & PdDeviceFusb302) == PdDeviceFusb302;

    if(device) {
        *device = instance->device;
    }
    if(!initialized) {
        FURI_LOG_E(TAG, "PD device not initialized");
    }
    return initialized;
}

int32_t pd_srv(void* p) {
    UNUSED(p);

    Pd* instance = pd_alloc();
    furi_event_loop_run(instance->event_loop);

    return 0;
}

bool pd_set_mode(Pd* instance, PdMode mode) {
    furi_check(instance);
    furi_check(mode < PdModeCount);
    if(pd_is_device_initialized(instance, NULL)) {
        const PdMessage msg = {
            .type = PdMessageTypeSetMode,
            .set_mode = mode,
        };

        pd_send_message(instance, &msg);
        return true;
    }
    return false;
}

bool pd_get_mode(Pd* instance, PdMode* mode) {
    furi_check(instance);
    if(pd_is_device_initialized(instance, NULL)) {
        PdMessage msg = {
            .type = PdMessageTypeGetMode,
            .get_mode = mode,
            .lock = api_lock_alloc_locked(),
        };

        pd_send_message(instance, &msg);
        return true;
    }
    return false;
}

bool pd_reset_config(Pd* instance) {
    furi_check(instance);
    if(pd_is_device_initialized(instance, NULL)) {
        PdMessage msg = {
            .type = PdMessageTypeResetConfig,
            .lock = api_lock_alloc_locked(),
        };

        pd_send_message(instance, &msg);
        return true;
    }
    return false;
}

FuriPubSub* pd_get_pubsub(Pd* pd) {
    furi_check(pd);
    return pd->event_pubsub;
}
