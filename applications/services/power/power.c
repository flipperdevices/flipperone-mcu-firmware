#include "power.h"

#include "furi_bsp_expander.h"
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

typedef enum {
    // Ina219
    PowerMessageTypeIna219GetVoltageV,
    PowerMessageTypeIna219GetCurrentA,
    PowerMessageTypeIna219GetPowerW,
    PowerMessageTypeIna219GetShuntVoltageMv,
    // Bq25792
    PowerMessageTypeBq25792SetPowerSwitch,
    PowerMessageTypeBq25792GetIbusMa,
    PowerMessageTypeBq25792GetIbatMa,
    PowerMessageTypeBq25792GetVbusMv,
    PowerMessageTypeBq25792GetVbatMv,
    PowerMessageTypeBq25792GetVsysMv,
    PowerMessageTypeBq25792GetChargerTemperature,
    PowerMessageTypeBq25792GetTemperatureBatteryCelsius,
    PowerMessageTypeBq25792GetInputCurrentLimitMa,
    PowerMessageTypeBq25792SetInputCurrentLimitMa,
    PowerMessageTypeBq25792GetChargeVoltageLimitMa,
    PowerMessageTypeBq25792SetChargeVoltageLimitMa,
    PowerMessageTypeBq25792GetChargeCurrentLimitMa,
    PowerMessageTypeBq25792SetChargeCurrentLimitMa,
    PowerMessageTypeBq25792ChargeEnable,
    PowerMessageTypeBq25792GetChargerStatus,
    PowerMessageTypeBq25792GetChargerFault,
    PowerMessageTypeBq25792GetChargerIrqFlags,
    PowerMessageTypeBq25792AdcEnable,
    PowerMessageTypeBq25792WatchdogReset,
} PowerMessageType;

typedef struct {
    PowerMessageType type;
    FuriApiLock lock;
    bool* result;
    union {
        // Ina219
        float_t* get_voltage_v;
        float_t* get_current_a;
        float_t* get_power_w;
        float_t* get_shunt_voltage_mv;
        // Bq25792
        Bq25792PowerSwitch power_switch;
        int16_t* get_ibus_ma;
        int16_t* get_ibat_ma;
        uint16_t* get_vbus_mv;
        uint16_t* get_vbat_mv;
        uint16_t* get_vsys_mv;
        float* get_charger_temperature;
        float* get_temperature_battery_celsius;
        uint16_t* get_input_current_limit_ma;
        uint16_t set_input_current_limit_ma;
        uint16_t* get_charge_voltage_limit_ma;
        uint16_t set_charge_voltage_limit_ma;
        uint16_t* get_charge_current_limit_ma;
        uint16_t set_charge_current_limit_ma;
        bool set_charge_enable;
        Bq25792ChargerStatusReg* get_charger_status;
        Bq25792FaultStatusReg* get_charger_fault;
        Bq25792ChargerFlagReg* get_charger_irq_flags;
        bool set_adc_enable;
    };

} PowerMessage;

static void __isr __not_in_flash_func(power_bq25792_event_isr)(void* context) {
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

    case PowerMessageTypeBq25792SetPowerSwitch:
        result = bq25792_set_power_switch(instance->bq25792_header, msg.power_switch) == Bq25792StatusOk;
        break;
    case PowerMessageTypeBq25792GetIbusMa:
        result = bq25792_get_ibus_ma(instance->bq25792_header, msg.get_ibus_ma) == Bq25792StatusOk;
        break;
    case PowerMessageTypeBq25792GetIbatMa:
        result = bq25792_get_ibat_ma(instance->bq25792_header, msg.get_ibat_ma) == Bq25792StatusOk;
        break;
    case PowerMessageTypeBq25792GetVbusMv:
        result = bq25792_get_vbus_mv(instance->bq25792_header, msg.get_vbus_mv) == Bq25792StatusOk;
        break;
    case PowerMessageTypeBq25792GetVbatMv:
        result = bq25792_get_vbat_mv(instance->bq25792_header, msg.get_vbat_mv) == Bq25792StatusOk;
        break;
    case PowerMessageTypeBq25792GetVsysMv:
        result = bq25792_get_vsys_mv(instance->bq25792_header, msg.get_vsys_mv) == Bq25792StatusOk;
        break;
    case PowerMessageTypeBq25792GetChargerTemperature:
        result = bq25792_get_charger_temperature(instance->bq25792_header, msg.get_charger_temperature) == Bq25792StatusOk;
        break;
    case PowerMessageTypeBq25792GetTemperatureBatteryCelsius:
        result = bq25792_get_temperature_battery_celsius(instance->bq25792_header, msg.get_temperature_battery_celsius) == Bq25792StatusOk;
        break;
    case PowerMessageTypeBq25792GetInputCurrentLimitMa:
        result = bq25792_get_input_current_limit_ma(instance->bq25792_header, msg.get_input_current_limit_ma) == Bq25792StatusOk;
        break;
    case PowerMessageTypeBq25792SetInputCurrentLimitMa:
        result = bq25792_set_input_current_limit_ma(instance->bq25792_header, msg.set_input_current_limit_ma) == Bq25792StatusOk;
        break;
    case PowerMessageTypeBq25792GetChargeVoltageLimitMa:
        result = bq25792_get_charge_voltage_limit_ma(instance->bq25792_header, msg.get_charge_voltage_limit_ma) == Bq25792StatusOk;
        break;
    case PowerMessageTypeBq25792SetChargeVoltageLimitMa:
        result = bq25792_set_charge_voltage_limit_ma(instance->bq25792_header, msg.set_charge_voltage_limit_ma) == Bq25792StatusOk;
        break;
    case PowerMessageTypeBq25792GetChargeCurrentLimitMa:
        result = bq25792_get_charge_current_limit_ma(instance->bq25792_header, msg.get_charge_current_limit_ma) == Bq25792StatusOk;
        break;
    case PowerMessageTypeBq25792SetChargeCurrentLimitMa:
        result = bq25792_set_charge_current_limit_ma(instance->bq25792_header, msg.set_charge_current_limit_ma) == Bq25792StatusOk;
        break;
    case PowerMessageTypeBq25792ChargeEnable:
        result = bq25792_charge_enable(instance->bq25792_header, msg.set_charge_enable) == Bq25792StatusOk;
        break;
    case PowerMessageTypeBq25792GetChargerStatus:
        result = bq25792_get_charger_status(instance->bq25792_header, msg.get_charger_status) == Bq25792StatusOk;
        break;
    case PowerMessageTypeBq25792GetChargerFault:
        result = bq25792_get_charger_fault(instance->bq25792_header, msg.get_charger_fault) == Bq25792StatusOk;
        break;
    case PowerMessageTypeBq25792GetChargerIrqFlags:
        result = bq25792_get_charger_irq_flags(instance->bq25792_header, msg.get_charger_irq_flags) == Bq25792StatusOk;
        break;
    case PowerMessageTypeBq25792AdcEnable:
        result = bq25792_adc_enable(instance->bq25792_header, msg.set_adc_enable) == Bq25792StatusOk;
        break;
    case PowerMessageTypeBq25792WatchdogReset:
        result = bq25792_watchdog_reset(instance->bq25792_header) == Bq25792StatusOk;
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
    instance->bq25792_header = bq25792_init(&furi_hal_i2c_handle_main, BQ25792_ADDRESS, NULL);
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

bool power_bq25792_set_power_switch(Power* instance, Bq25792PowerSwitch power_switch) {
    furi_check(instance);
    bool result;
    PowerMessage msg = {
        .type = PowerMessageTypeBq25792SetPowerSwitch,
        .power_switch = power_switch,
        .result = &result,
        .lock = api_lock_alloc_locked(),
    };
    power_send_message(instance, &msg);
    return result;
}

bool power_bq25792_get_ibus_ma(Power* instance, int16_t* ibus) {
    furi_check(instance);
    bool result;
    PowerMessage msg = {
        .type = PowerMessageTypeBq25792GetIbusMa,
        .get_ibus_ma = ibus,
        .result = &result,
        .lock = api_lock_alloc_locked(),
    };
    power_send_message(instance, &msg);
    return result;
}

bool power_bq25792_get_ibat_ma(Power* instance, int16_t* ibat) {
    furi_check(instance);
    bool result;
    PowerMessage msg = {
        .type = PowerMessageTypeBq25792GetIbatMa,
        .get_ibat_ma = ibat,
        .result = &result,
        .lock = api_lock_alloc_locked(),
    };
    power_send_message(instance, &msg);
    return result;
}

bool power_bq25792_get_vbus_mv(Power* instance, uint16_t* vbus) {
    furi_check(instance);
    bool result;
    PowerMessage msg = {
        .type = PowerMessageTypeBq25792GetVbusMv,
        .get_vbus_mv = vbus,
        .result = &result,
        .lock = api_lock_alloc_locked(),
    };
    power_send_message(instance, &msg);
    return result;
}

bool power_bq25792_get_vbat_mv(Power* instance, uint16_t* vbat) {
    furi_check(instance);
    bool result;
    PowerMessage msg = {
        .type = PowerMessageTypeBq25792GetVbatMv,
        .get_vbat_mv = vbat,
        .result = &result,
        .lock = api_lock_alloc_locked(),
    };
    power_send_message(instance, &msg);
    return result;
}

bool power_bq25792_get_vsys_mv(Power* instance, uint16_t* vsys) {
    furi_check(instance);
    bool result;
    PowerMessage msg = {
        .type = PowerMessageTypeBq25792GetVsysMv,
        .get_vsys_mv = vsys,
        .result = &result,
        .lock = api_lock_alloc_locked(),
    };
    power_send_message(instance, &msg);
    return result;
}

bool power_bq25792_get_charger_temperature(Power* instance, float* temperature) {
    furi_check(instance);
    bool result;
    PowerMessage msg = {
        .type = PowerMessageTypeBq25792GetChargerTemperature,
        .get_charger_temperature = temperature,
        .result = &result,
        .lock = api_lock_alloc_locked(),
    };
    power_send_message(instance, &msg);
    return result;
}

bool power_bq25792_get_temperature_battery_celsius(Power* instance, float* temperature) {
    furi_check(instance);
    bool result;
    PowerMessage msg = {
        .type = PowerMessageTypeBq25792GetTemperatureBatteryCelsius,
        .get_temperature_battery_celsius = temperature,
        .result = &result,
        .lock = api_lock_alloc_locked(),
    };
    power_send_message(instance, &msg);
    return result;
}

bool power_bq25792_get_input_current_limit_ma(Power* instance, uint16_t* input_current_limit) {
    furi_check(instance);
    bool result;
    PowerMessage msg = {
        .type = PowerMessageTypeBq25792GetInputCurrentLimitMa,
        .get_input_current_limit_ma = input_current_limit,
        .result = &result,
        .lock = api_lock_alloc_locked(),
    };
    power_send_message(instance, &msg);
    return result;
}

bool power_bq25792_set_input_current_limit_ma(Power* instance, uint16_t input_current_limit) {
    furi_check(instance);
    bool result;
    PowerMessage msg = {
        .type = PowerMessageTypeBq25792SetInputCurrentLimitMa,
        .set_input_current_limit_ma = input_current_limit,
        .result = &result,
        .lock = api_lock_alloc_locked(),
    };
    power_send_message(instance, &msg);
    return result;
}

bool power_bq25792_get_charge_voltage_limit_ma(Power* instance, uint16_t* charge_voltage_limit) {
    furi_check(instance);
    bool result;
    PowerMessage msg = {
        .type = PowerMessageTypeBq25792GetChargeVoltageLimitMa,
        .get_charge_voltage_limit_ma = charge_voltage_limit,
        .result = &result,
        .lock = api_lock_alloc_locked(),
    };
    power_send_message(instance, &msg);
    return result;
}

bool power_bq25792_set_charge_voltage_limit_ma(Power* instance, uint16_t charge_voltage_limit) {
    furi_check(instance);
    bool result;
    PowerMessage msg = {
        .type = PowerMessageTypeBq25792SetChargeVoltageLimitMa,
        .set_charge_voltage_limit_ma = charge_voltage_limit,
        .result = &result,
        .lock = api_lock_alloc_locked(),
    };
    power_send_message(instance, &msg);
    return result;
}

bool power_bq25792_get_charge_current_limit_ma(Power* instance, uint16_t* charge_current_limit) {
    furi_check(instance);
    bool result;
    PowerMessage msg = {
        .type = PowerMessageTypeBq25792GetChargeCurrentLimitMa,
        .get_charge_current_limit_ma = charge_current_limit,
        .result = &result,
        .lock = api_lock_alloc_locked(),
    };
    power_send_message(instance, &msg);
    return result;
}

bool power_bq25792_set_charge_current_limit_ma(Power* instance, uint16_t charge_current_limit) {
    furi_check(instance);
    bool result;
    PowerMessage msg = {
        .type = PowerMessageTypeBq25792SetChargeCurrentLimitMa,
        .set_charge_current_limit_ma = charge_current_limit,
        .result = &result,
        .lock = api_lock_alloc_locked(),
    };
    power_send_message(instance, &msg);
    return result;
}

bool power_bq25792_charge_enable(Power* instance, bool enable) {
    furi_check(instance);
    bool result;
    PowerMessage msg = {
        .type = PowerMessageTypeBq25792ChargeEnable,
        .set_charge_enable = enable,
        .result = &result,
        .lock = api_lock_alloc_locked(),
    };
    power_send_message(instance, &msg);
    return result;
}

bool power_bq25792_get_charger_status(Power* instance, Bq25792ChargerStatusReg* status) {
    furi_check(instance);
    bool result;
    PowerMessage msg = {
        .type = PowerMessageTypeBq25792GetChargerStatus,
        .get_charger_status = status,
        .result = &result,
        .lock = api_lock_alloc_locked(),
    };
    power_send_message(instance, &msg);
    return result;
}

bool power_bq25792_get_charger_fault(Power* instance, Bq25792FaultStatusReg* fault) {
    furi_check(instance);
    bool result;
    PowerMessage msg = {
        .type = PowerMessageTypeBq25792GetChargerFault,
        .get_charger_fault = fault,
        .result = &result,
        .lock = api_lock_alloc_locked(),
    };
    power_send_message(instance, &msg);
    return result;
}

bool power_bq25792_get_charger_irq_flags(Power* instance, Bq25792ChargerFlagReg* irq_flags) {
    furi_check(instance);
    bool result;
    PowerMessage msg = {
        .type = PowerMessageTypeBq25792GetChargerIrqFlags,
        .get_charger_irq_flags = irq_flags,
        .result = &result,
        .lock = api_lock_alloc_locked(),
    };
    power_send_message(instance, &msg);
    return result;
}

bool power_bq25792_adc_enable(Power* instance, bool enable) {
    furi_check(instance);
    bool result;
    PowerMessage msg = {
        .type = PowerMessageTypeBq25792AdcEnable,
        .set_adc_enable = enable,
        .result = &result,
        .lock = api_lock_alloc_locked(),
    };
    power_send_message(instance, &msg);
    return result;
}

bool power_bq25792_watchdog_reset(Power* instance) {
    furi_check(instance);
    bool result;
    PowerMessage msg = {
        .type = PowerMessageTypeBq25792WatchdogReset,
        .result = &result,
        .lock = api_lock_alloc_locked(),
    };
    power_send_message(instance, &msg);
    return result;
}
