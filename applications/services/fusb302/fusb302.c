#include "fusb302.h"

#include <furi.h>
#include <api_lock.h>
#include <furi_hal_i2c_config.h>
#include <furi_hal_resources.h>
#include <drivers/fusb302/fusb302.h>
#include <furi_bsp.h>

#define TAG "Fusb302"

#define FUSB302_MAX_MESSAGES (8)

typedef enum {
    Fusb302EventTypeIsr = (1 << 0),
    Fusb302EventTypeAll = (Fusb302EventTypeIsr),
} Fusb302EventType;

struct Fusb302 {
    FuriEventLoop* event_loop;
    FuriPubSub* event_pubsub;
    Fusb302* fusb302_header;
    Fusb302Mode mode;
    FuriMessageQueue* message_queue;
};

typedef enum {
    Fusb302MessageTypeSetMode,
    Fusb302MessageTypeGetMode,
} Fusb302MessageType;

typedef struct {
    Fusb302MessageType type;
    FuriApiLock lock;
    bool* result;
    union {
        Fusb302Mode* get_mode;
        Fusb302Mode set_mode;
    };
} Fusb302Message;

static void __isr __not_in_flash_func(fusb302_event_isr)(void* context) {
    Fusb302* instance = (Fusb302*)context;
    furi_event_loop_set_custom_event(instance->event_loop, Fusb302EventTypeIsr);
}

static void fusb302_message_queue_callback(FuriEventLoopObject* object, void* context) {
    furi_assert(context);
    Fusb302* instance = context;
    furi_assert(object == instance->message_queue);

    Fusb302Message msg;
    furi_check(furi_message_queue_get(instance->message_queue, &msg, 0) == FuriStatusOk);

    bool result = false;

    switch(msg.type) {
    case Fusb302MessageTypeSetMode:
        instance->mode = msg.set_mode;
        break;
    case Fusb302MessageTypeGetMode:
        *(msg.get_mode) = instance->mode;
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

static void fusb302_custom_event_callback(uint32_t events, void* context) {
    furi_assert(context);
    Fusb302* instance = (Fusb302*)context;

    if(events & Fusb302EventTypeIsr) {
        fusb302_read_role(instance->fusb302_header);


        // fusb302_pd_reset_hard(instance->fusb302_header);
        
        // fusb302_cc_orientation_set(instance->fusb302_header, Fusb302TypeCcOrientationNormal);
        // fusb302_pd_reset_logic(instance->fusb302_header);
        // fusb302_pd_autogoodcrc_set(instance->fusb302_header, true);
        // fusb302_pd_autoretry_set(instance->fusb302_header, 3);
        // fusb302_pd_rx_flush(instance->fusb302_header);

        // for(uint8_t i = 0;i<10; i++) {
        //     Fusb302PdMsg msg;
        //     Fusb302Status res = fusb302_pd_message_receive(instance->fusb302_header, &msg);

        //     if(res == Fusb302StatusOk) {
        //         FURI_LOG_W(TAG, "Received PD message: SOP=%d, Header=0x%04X, Objects=%d", msg.sop_type, msg.header, msg.object_count);
        //     } else if(res == Fusb302StatusRxEmpty) {
        //         FURI_LOG_W(TAG, "No PD message received (Rx FIFO empty)");
        //         break;
        //     } else {
        //         FURI_LOG_W(TAG, "Error receiving PD message");
        //         break;
        //     }
        //     furi_delay_ms(50);
        // }

    }
}

static void fusb302_send_message(Fusb302* instance, const Fusb302Message* message) {
    furi_check(furi_message_queue_put(instance->message_queue, message, FuriWaitForever) == FuriStatusOk);

    if(message->lock) {
        api_lock_wait_unlock_and_free(message->lock);
    }
}

static Fusb302* fusb302_alloc(void) {
    Fusb302* instance = (Fusb302*)malloc(sizeof(Fusb302));
    instance->event_loop = furi_event_loop_alloc();
    instance->message_queue = furi_message_queue_alloc(FUSB302_MAX_MESSAGES, sizeof(Fusb302Message));
    instance->fusb302_header = fusb302_init(&furi_hal_i2c_handle_external, FUSB302_ADDRESS, NULL);
    furi_bsp_expander_main_attach_fusb302_callback(fusb302_event_isr, instance);
    instance->mode = Fusb302ModeOff;
    
    furi_event_loop_subscribe_message_queue(instance->event_loop, instance->message_queue, FuriEventLoopEventIn, fusb302_message_queue_callback, instance);
    furi_event_loop_set_custom_event_callback(instance->event_loop, fusb302_custom_event_callback, instance);

    instance->event_pubsub = furi_pubsub_alloc();
    furi_record_create(RECORD_FUSB302, instance);

    return instance;
}

int32_t fusb302_srv(void* p) {
    UNUSED(p);

    Fusb302* instance = fusb302_alloc();
    furi_event_loop_run(instance->event_loop);

    return 0;
}

void fusb302_set_mode(Fusb302* instance, Fusb302Mode mode) {
    furi_check(instance);
    furi_check(mode < Fusb302ModeCount);
    const Fusb302Message msg = {
        .type = Fusb302MessageTypeSetMode,
        .set_mode = mode,
    };

    fusb302_send_message(instance, &msg);
}

Fusb302Mode fusb302_get_mode(Fusb302* instance) {
    furi_check(instance);

    Fusb302Mode mode;
    Fusb302Message msg = {
        .type = Fusb302MessageTypeGetMode,
        .get_mode = &mode,
        .lock = api_lock_alloc_locked(),
    };

    fusb302_send_message(instance, &msg);

    return mode;
}

FuriPubSub* fusb302_get_pubsub(Fusb302* fusb302) {
    furi_check(fusb302);
    return fusb302->event_pubsub;
}
