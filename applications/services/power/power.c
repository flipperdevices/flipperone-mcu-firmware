#include "power.h"

#include "furi_bsp_expander.h"
#include <furi.h>
#include <api_lock.h>
#include <furi_hal_i2c_config.h>
#include <furi_hal_resources.h>
#include <drivers/ina219/ina219.h>
#include <drivers/bq25792/bq25792.h>
#include <furi_bsp.h>
#include <math.h>

#define TAG "Power"

#define POWER_MAX_MESSAGES            (8)
#define POWER_INA_SHUNT_RESISTOR_OHMS (0.004f)
#define POWER_INA_BUS_CURRENT_MAX     (9.0f)

typedef enum {
    PowerEventTypeIsr = (1 << 0),
    PowerEventTypeAll = (PowerEventTypeIsr),
} PowerEventType;

struct Power {
    FuriEventLoop* event_loop;
    FuriPubSub* event_pubsub;
    Bq25792* bq25792_header;
    Ina219* ina219_header;
    //PowerMode mode;
    FuriMessageQueue* message_queue;
};

typedef enum {
    PowerMessageTypeIna219GetVoltageV,
    PowerMessageTypeIna219GetCurrentA,
    PowerMessageTypeIna219GetPowerW,
    PowerMessageTypeIna219GetShuntVoltageMv,
} PowerMessageType;

typedef struct {
    PowerMessageType type;
    FuriApiLock lock;
    bool* result;
    union {
        float_t* get_voltage_v;
        float_t* get_current_a;
        float_t* get_power_w;
        float_t* get_shunt_voltage_mv;
        // PowerMode* get_mode;
        // PowerMode set_mode;
    };
} PowerMessage;

static void __isr __not_in_flash_func(power_event_isr)(void* context) {
    Power* instance = (Power*)context;
    furi_event_loop_set_custom_event(instance->event_loop, PowerEventTypeIsr);
}

static void power_message_queue_callback(FuriEventLoopObject* object, void* context) {
    furi_assert(context);
    Power* instance = context;
    furi_assert(object == instance->message_queue);

    PowerMessage msg;
    furi_check(furi_message_queue_get(instance->message_queue, &msg, 0) == FuriStatusOk);

    bool result = false;

    switch(msg.type) {
    case PowerMessageTypeIna219GetVoltageV:
        *(msg.get_voltage_v) = ina219_get_bus_voltage_v(instance->ina219_header);
        break;
    case PowerMessageTypeIna219GetCurrentA:
        *(msg.get_current_a) = ina219_get_current_a(instance->ina219_header);
        break;
    case PowerMessageTypeIna219GetPowerW:
        *(msg.get_power_w) = ina219_get_power_w(instance->ina219_header);
        break;
    case PowerMessageTypeIna219GetShuntVoltageMv:
        *(msg.get_shunt_voltage_mv) = ina219_get_shunt_voltage_mv(instance->ina219_header);
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

static void power_custom_event_callback(uint32_t events, void* context) {
    furi_assert(context);
    Power* instance = (Power*)context;

    if(events & PowerEventTypeIsr) {
    }
}

static void power_send_message(Power* instance, const PowerMessage* message) {
    furi_check(furi_message_queue_put(instance->message_queue, message, FuriWaitForever) == FuriStatusOk);

    if(message->lock) {
        api_lock_wait_unlock_and_free(message->lock);
    }
}

static Power* power_alloc(void) {
    Power* instance = (Power*)malloc(sizeof(Power));
    instance->event_loop = furi_event_loop_alloc();
    instance->message_queue = furi_message_queue_alloc(POWER_MAX_MESSAGES, sizeof(PowerMessage));
    instance->bq25792_header = bq25792_init(&furi_hal_i2c_handle_external, BQ25792_ADDRESS, NULL);
    instance->ina219_header = ina219_init(&furi_hal_i2c_handle_external, INA219_ADDRESS, POWER_INA_SHUNT_RESISTOR_OHMS, POWER_INA_BUS_CURRENT_MAX);

    if(!instance->bq25792_header) {
        FURI_LOG_E(TAG, "Failed to initialize BQ25792");
    } else {
        furi_bsp_expander_main_attach_bq25792_callback(power_event_isr, instance);
    }
    if(!instance->ina219_header) {
        FURI_LOG_E(TAG, "Failed to initialize INA219");
    }

    furi_event_loop_subscribe_message_queue(instance->event_loop, instance->message_queue, FuriEventLoopEventIn, power_message_queue_callback, instance);
    furi_event_loop_set_custom_event_callback(instance->event_loop, power_custom_event_callback, instance);

    instance->event_pubsub = furi_pubsub_alloc();
    furi_record_create(RECORD_POWER, instance);

    return instance;
}

int32_t power_srv(void* p) {
    UNUSED(p);

    Power* instance = power_alloc();
    furi_event_loop_run(instance->event_loop);

    return 0;
}

FuriPubSub* power_get_pubsub(Power* power) {
    furi_check(power);
    return power->event_pubsub;
}


// void power_set_mode(Power* instance) {
//     furi_check(instance);
//     //furi_check(mode < PowerModeCount);
//     const PowerMessage msg = {
//         .type = PowerMessageTypeSetMode,
//         //.set_mode = mode,
//     };

//     power_send_message(instance, &msg);
// }

float_t power_ina219_get_voltage_v(Power* instance) {
    furi_check(instance);
    float_t voltage;
    PowerMessage msg = {
        .type = PowerMessageTypeIna219GetVoltageV,
        .get_voltage_v = &voltage,
        .lock = api_lock_alloc_locked(),
    };
    power_send_message(instance, &msg);
    return voltage;
}

float_t power_ina219_get_current_a(Power* instance) {
    furi_check(instance);
    float_t current;
    PowerMessage msg = {
        .type = PowerMessageTypeIna219GetCurrentA,
        .get_current_a = &current,
        .lock = api_lock_alloc_locked(),
    };
    power_send_message(instance, &msg);
    return current;
}

float_t power_ina219_get_power_w(Power* instance) {
    furi_check(instance);
    float_t power;
    PowerMessage msg = {
        .type = PowerMessageTypeIna219GetPowerW,
        .get_power_w = &power,
        .lock = api_lock_alloc_locked(),
    };
    power_send_message(instance, &msg);
    return power;
}

float_t power_ina219_get_shunt_voltage_mv(Power* instance) {
    furi_check(instance);
    float_t shunt_voltage;
    PowerMessage msg = {
        .type = PowerMessageTypeIna219GetShuntVoltageMv,
        .get_shunt_voltage_mv = &shunt_voltage,
        .lock = api_lock_alloc_locked(),
    };
    power_send_message(instance, &msg);
    return shunt_voltage;
}
