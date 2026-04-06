#include "power.h"

#include <furi_bsp_expander.h>
#include <furi.h>
#include <api_lock.h>
#include <furi_hal_i2c_config.h>
#include <furi_hal_resources.h>
#include <drivers/ina219/ina219.h>
#include <drivers/bq25792/bq25792.h>
#include <furi_bsp.h>

#define TAG "Power"

#define POWER_MAX_MESSAGES            (8)
#define POWER_INA_SHUNT_RESISTOR_OHMS (0.004f)
#define POWER_INA_BUS_CURRENT_MAX     (9.0f)

#define BQ25792_BAT_MAX_CHARGE_VOLTAGE 8800
#define BQ25792_BAT_MAX_CHARGE_CURRENT 3000
#define BQ25792_BAT_MAX_INPUT_CURRENT  3000

typedef enum {
    PowerEventTypeIsr = (1 << 0),
    PowerEventTypeAll = (PowerEventTypeIsr),
} PowerEventType;

struct Power {
    FuriEventLoop* event_loop;
    FuriPubSub* event_pubsub;
    Bq25792* bq25792_header;
    Ina219* ina219_header;
    FuriMessageQueue* message_queue;
};

static void __isr __not_in_flash_func(power_bq25792_event_isr)(void* context) {
    Power* instance = (Power*)context;
    furi_event_loop_set_custom_event(instance->event_loop, PowerEventTypeIsr);
}

typedef void (*PowerFunction)(void* context, void* param, void* result);

typedef struct {
    FuriApiLock lock;
    PowerFunction function;
    void* context;
    void* param;
    void* result;
} PowerMessage;

#define API_WRAPPER(func, return_type, context_type)            \
    void func##_api(void* context, void* param, void* result) { \
        if(0) {                                                 \
            /* typechecking */                                  \
            return_type r = func((context_type)0);              \
        }                                                       \
        *(return_type*)result = func((context_type)context);    \
    }

#define API_WRAPPER_PARAM(func, return_type, context_type, param_type)            \
    void func##_api(void* context, void* param, void* result) {                   \
        if(0) {                                                                   \
            /* typechecking */                                                    \
            return_type r = func((context_type)0, (param_type)0);                 \
        }                                                                         \
        *(return_type*)result = func((context_type)context, *(param_type*)param); \
    }

// Ina219 wrappers

API_WRAPPER(ina219_get_bus_voltage_v, float_t, Ina219*);
API_WRAPPER(ina219_get_current_a, float_t, Ina219*);
API_WRAPPER(ina219_get_power_w, float_t, Ina219*);
API_WRAPPER(ina219_get_shunt_voltage_mv, float_t, Ina219*);

// Bq25792 wrappers

API_WRAPPER_PARAM(bq25792_set_power_switch, Bq25792Status, Bq25792*, Bq25792PowerSwitch);
API_WRAPPER_PARAM(bq25792_get_ibus_ma, Bq25792Status, Bq25792*, int16_t*);
API_WRAPPER_PARAM(bq25792_get_ibat_ma, Bq25792Status, Bq25792*, int16_t*);
API_WRAPPER_PARAM(bq25792_get_vbus_mv, Bq25792Status, Bq25792*, uint16_t*);
API_WRAPPER_PARAM(bq25792_get_vbat_mv, Bq25792Status, Bq25792*, uint16_t*);
API_WRAPPER_PARAM(bq25792_get_vsys_mv, Bq25792Status, Bq25792*, uint16_t*);
API_WRAPPER_PARAM(bq25792_get_charger_temperature, Bq25792Status, Bq25792*, float*);
API_WRAPPER_PARAM(bq25792_get_temperature_battery_celsius, Bq25792Status, Bq25792*, float*);
API_WRAPPER_PARAM(bq25792_get_input_current_limit_ma, Bq25792Status, Bq25792*, uint16_t*);
API_WRAPPER_PARAM(bq25792_set_input_current_limit_ma, Bq25792Status, Bq25792*, uint16_t);
API_WRAPPER_PARAM(bq25792_get_charge_voltage_limit_ma, Bq25792Status, Bq25792*, uint16_t*);
API_WRAPPER_PARAM(bq25792_set_charge_voltage_limit_ma, Bq25792Status, Bq25792*, uint16_t);
API_WRAPPER_PARAM(bq25792_get_charge_current_limit_ma, Bq25792Status, Bq25792*, uint16_t*);
API_WRAPPER_PARAM(bq25792_set_charge_current_limit_ma, Bq25792Status, Bq25792*, uint16_t);
API_WRAPPER_PARAM(bq25792_get_ico_current_limit_ma, Bq25792Status, Bq25792*, uint16_t*);
API_WRAPPER_PARAM(bq25792_charge_enable, Bq25792Status, Bq25792*, bool);
API_WRAPPER_PARAM(bq25792_get_charger_status, Bq25792Status, Bq25792*, Bq25792ChargerStatusReg*);
API_WRAPPER_PARAM(bq25792_get_charger_fault, Bq25792Status, Bq25792*, Bq25792FaultStatusReg*);
API_WRAPPER_PARAM(bq25792_get_charger_irq_flags, Bq25792Status, Bq25792*, Bq25792ChargerFlagReg*);
API_WRAPPER_PARAM(bq25792_adc_enable, Bq25792Status, Bq25792*, bool);
API_WRAPPER(bq25792_watchdog_reset, Bq25792Status, Bq25792*);

// End of API wrappers

static void power_message_queue_callback(FuriEventLoopObject* object, void* context) {
    furi_assert(context);
    Power* instance = context;
    furi_assert(object == instance->message_queue);

    PowerMessage msg;
    furi_check(furi_message_queue_get(instance->message_queue, &msg, 0) == FuriStatusOk);

    furi_check(msg.function);
    furi_check(msg.result);

    msg.function(msg.context, msg.param, msg.result);

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
    instance->bq25792_header = bq25792_init(&furi_hal_i2c_handle_main, BQ25792_ADDRESS, NULL);

    // Set default charge voltage and current limits
    bq25792_set_charge_voltage_limit_ma(instance->bq25792_header, BQ25792_BAT_MAX_CHARGE_VOLTAGE);
    bq25792_set_charge_current_limit_ma(instance->bq25792_header, BQ25792_BAT_MAX_CHARGE_CURRENT);
    bq25792_set_input_current_limit_ma(instance->bq25792_header, BQ25792_BAT_MAX_INPUT_CURRENT);

    instance->ina219_header = ina219_init(&furi_hal_i2c_handle_main, INA219_ADDRESS, POWER_INA_SHUNT_RESISTOR_OHMS, POWER_INA_BUS_CURRENT_MAX);

    if(!instance->bq25792_header) {
        FURI_LOG_E(TAG, "Failed to initialize BQ25792");
    } else {
        furi_bsp_expander_main_attach_bq25792_callback(power_bq25792_event_isr, instance);
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

#define POWER_API_CALL_PARAM(fn, ctx, par, res) \
    do {                                        \
        if(0) {                                 \
            /* typechecking */                  \
            res = fn(ctx, par);                 \
        }                                       \
        PowerMessage msg = {                    \
            .function = fn##_api,               \
            .context = ctx,                     \
            .param = &(par),                    \
            .result = &(res),                   \
            .lock = api_lock_alloc_locked(),    \
        };                                      \
        power_send_message(instance, &msg);     \
    } while(0);

#define POWER_API_CALL(fn, ctx, res)         \
    do {                                     \
        if(0) {                              \
            /* typechecking */               \
            res = fn(ctx);                   \
        }                                    \
        PowerMessage msg = {                 \
            .function = fn##_api,            \
            .context = ctx,                  \
            .param = NULL,                   \
            .result = &(res),                \
            .lock = api_lock_alloc_locked(), \
        };                                   \
        power_send_message(instance, &msg);  \
    } while(0);

// Ina219 API functions

float_t power_ina219_get_voltage_v(Power* instance) {
    furi_check(instance);
    float_t voltage;
    POWER_API_CALL(ina219_get_bus_voltage_v, instance->ina219_header, voltage);
    return voltage;
}

float_t power_ina219_get_current_a(Power* instance) {
    furi_check(instance);
    float_t current;
    POWER_API_CALL(ina219_get_current_a, instance->ina219_header, current);
    return current;
}

float_t power_ina219_get_power_w(Power* instance) {
    furi_check(instance);
    float_t power;
    POWER_API_CALL(ina219_get_power_w, instance->ina219_header, power);
    return power;
}

float_t power_ina219_get_shunt_voltage_mv(Power* instance) {
    furi_check(instance);
    float_t shunt_voltage;
    POWER_API_CALL(ina219_get_shunt_voltage_mv, instance->ina219_header, shunt_voltage);
    return shunt_voltage;
}

// Bq25792 API functions

bool power_bq25792_set_power_switch(Power* instance, Bq25792PowerSwitch power_switch) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(bq25792_set_power_switch, instance->bq25792_header, power_switch, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_get_ibus_ma(Power* instance, int16_t* ibus) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(bq25792_get_ibus_ma, instance->bq25792_header, ibus, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_get_ibat_ma(Power* instance, int16_t* ibat) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(bq25792_get_ibat_ma, instance->bq25792_header, ibat, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_get_vbus_mv(Power* instance, uint16_t* vbus) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(bq25792_get_vbus_mv, instance->bq25792_header, vbus, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_get_vbat_mv(Power* instance, uint16_t* vbat) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(bq25792_get_vbat_mv, instance->bq25792_header, vbat, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_get_vsys_mv(Power* instance, uint16_t* vsys) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(bq25792_get_vsys_mv, instance->bq25792_header, vsys, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_get_charger_temperature(Power* instance, float* temperature) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(bq25792_get_charger_temperature, instance->bq25792_header, temperature, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_get_temperature_battery_celsius(Power* instance, float* temperature) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(bq25792_get_temperature_battery_celsius, instance->bq25792_header, temperature, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_get_input_current_limit_ma(Power* instance, uint16_t* input_current_limit) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(bq25792_get_input_current_limit_ma, instance->bq25792_header, input_current_limit, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_set_input_current_limit_ma(Power* instance, uint16_t input_current_limit) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(bq25792_set_input_current_limit_ma, instance->bq25792_header, input_current_limit, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_get_charge_voltage_limit_ma(Power* instance, uint16_t* charge_voltage_limit) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(bq25792_get_charge_voltage_limit_ma, instance->bq25792_header, charge_voltage_limit, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_set_charge_voltage_limit_ma(Power* instance, uint16_t charge_voltage_limit) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(bq25792_set_charge_voltage_limit_ma, instance->bq25792_header, charge_voltage_limit, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_get_charge_current_limit_ma(Power* instance, uint16_t* charge_current_limit) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(bq25792_get_charge_current_limit_ma, instance->bq25792_header, charge_current_limit, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_set_charge_current_limit_ma(Power* instance, uint16_t charge_current_limit) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(bq25792_set_charge_current_limit_ma, instance->bq25792_header, charge_current_limit, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_charge_enable(Power* instance, bool enable) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(bq25792_charge_enable, instance->bq25792_header, enable, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_get_charger_status(Power* instance, Bq25792ChargerStatusReg* status) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(bq25792_get_charger_status, instance->bq25792_header, status, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_get_charger_fault(Power* instance, Bq25792FaultStatusReg* fault) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(bq25792_get_charger_fault, instance->bq25792_header, fault, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_get_charger_irq_flags(Power* instance, Bq25792ChargerFlagReg* irq_flags) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(bq25792_get_charger_irq_flags, instance->bq25792_header, irq_flags, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_adc_enable(Power* instance, bool enable) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(bq25792_adc_enable, instance->bq25792_header, enable, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_watchdog_reset(Power* instance) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL(bq25792_watchdog_reset, instance->bq25792_header, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_get_ico_current_limit_ma(Power* instance, uint16_t* ico_current_limit) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(bq25792_get_ico_current_limit_ma, instance->bq25792_header, ico_current_limit, result);
    return result == Bq25792StatusOk;
}
