#include "power.h"

#include <furi_bsp_expander.h>
#include <furi.h>
#include <api_lock.h>
#include <furi_hal_i2c_config.h>
#include <furi_hal_resources.h>
#include <drivers/ina219/ina219.h>
#include <drivers/bq25792/bq25792.h>
#include <drivers/bq28z620/bq28z620.h>
#include <furi_bsp.h>

#define TAG "Power"

#define BQ25792_IRQ_DEBUG_ENABLE

#define POWER_MAX_MESSAGES            (8)
#define POWER_INA_SHUNT_RESISTOR_OHMS (0.004f)
#define POWER_INA_BUS_CURRENT_MAX     (9.0f)

#define BQ25792_BAT_MAX_CHARGE_VOLTAGE 8650
#define BQ25792_BAT_MAX_CHARGE_CURRENT 3000
#define BQ25792_BAT_MAX_INPUT_CURRENT  3000

#define BQ25792_OTG_WATCHDOG_TIME      Bq25792WatchdogTime0_5s
#define BQ25792_OTG_WATCHDOG_PERIOD_MS 350 // pet watchdog faster than 500 ms timeout

typedef enum {
    PowerEventTypeIsr = (1 << 0),
    PowerEventTypeAll = (PowerEventTypeIsr),
} PowerEventType;

struct Power {
    FuriEventLoop* event_loop;
    FuriPubSub* event_pubsub;
    Bq25792* bq25792_header;
    Ina219* ina219_header;
    Bq28z620* bq28z620_header;
    FuriMessageQueue* message_queue;
    PowerDevice devices;
    FuriEventLoopTimer* otg_watchdog_timer;
    volatile bool otg_enabled;
    FuriCallbackWithContext otg_overcurrent;
};

static Bq25792Status power_bq25792_reset_and_load_config(Power* instance) {
    furi_assert(instance);
    Bq25792Status res = Bq25792StatusUnknown;

    do {
        res = bq25792_load_default_config(instance->bq25792_header);
        if(res != Bq25792StatusOk) {
            FURI_LOG_E(TAG, "Failed to load BQ25792 default config: %d", res);
            break;
        }
        // Set default charge voltage and current limits
        res = bq25792_set_charge_voltage_limit_ma(instance->bq25792_header, BQ25792_BAT_MAX_CHARGE_VOLTAGE);
        if(res != Bq25792StatusOk) {
            FURI_LOG_E(TAG, "Failed to set BQ25792 charge voltage limit: %d", res);
            break;
        }
        res = bq25792_set_charge_current_limit_ma(instance->bq25792_header, BQ25792_BAT_MAX_CHARGE_CURRENT);
        if(res != Bq25792StatusOk) {
            FURI_LOG_E(TAG, "Failed to set BQ25792 charge current limit: %d", res);
            break;
        }
        res = bq25792_set_input_current_limit_ma(instance->bq25792_header, BQ25792_BAT_MAX_INPUT_CURRENT);
        if(res != Bq25792StatusOk) {
            FURI_LOG_E(TAG, "Failed to set BQ25792 input current limit: %d", res);
            break;
        }
    } while(false);
    return res;
}

#ifdef BQ25792_IRQ_DEBUG_ENABLE
static void power_bq25792_print_charger_irq(Power* instance, Bq25792ChargerFlagReg fl) {
    FuriString* arena = furi_string_alloc();
    furi_string_set(arena, "");
    if(fl.flag0.vbus_present_flag) furi_string_cat_printf(arena, " VBUS_PRESENT");
    if(fl.flag0.ac1_present_flag) furi_string_cat_printf(arena, " AC1_PRESENT");
    if(fl.flag0.ac2_present_flag) furi_string_cat_printf(arena, " AC2_PRESENT");
    if(fl.flag0.pg_flag) furi_string_cat_printf(arena, " PG");
    if(fl.flag0.poorsrc_flag) furi_string_cat_printf(arena, " POORSRC");
    if(fl.flag0.wd_flag) furi_string_cat_printf(arena, " WD");
    if(fl.flag0.vindpm_flag) furi_string_cat_printf(arena, " VINDPM");
    if(fl.flag0.iindpm_flag) furi_string_cat_printf(arena, " IINDPM");
    if(furi_string_size(arena) != 0) FURI_LOG_I(TAG, "  IRQ0:    0x%02X %s", fl.data[0], furi_string_get_cstr(arena));

    furi_string_set(arena, "");
    if(fl.flag1.bc12_done_flag) furi_string_cat_printf(arena, " BC1.2_DONE");
    if(fl.flag1.vbat_present_flag) furi_string_cat_printf(arena, " VBAT_PRESENT");
    if(fl.flag1.treg_flag) furi_string_cat_printf(arena, " TREG");
    if(fl.flag1.vbus_flag) furi_string_cat_printf(arena, " VBUS");
    if(fl.flag1.ico_flag) furi_string_cat_printf(arena, " ICO");
    if(fl.flag1.chg_flag) furi_string_cat_printf(arena, " CHG");
    if(furi_string_size(arena) != 0) FURI_LOG_I(TAG, "  IRQ1:    0x%02X %s", fl.data[1], furi_string_get_cstr(arena));

    furi_string_set(arena, "");
    if(fl.flag2.topoff_tmr_flag) furi_string_cat_printf(arena, " TOPOFF_TMR");
    if(fl.flag2.prechg_tmr_flag) furi_string_cat_printf(arena, " PRECHG_TMR");
    if(fl.flag2.trichg_tmr_flag) furi_string_cat_printf(arena, " TRICHG_TMR");
    if(fl.flag2.chg_tmr_flag) furi_string_cat_printf(arena, " CHG_TMR");
    if(fl.flag2.vsys_flag) furi_string_cat_printf(arena, " VSYS");
    if(fl.flag2.adc_done_flag) furi_string_cat_printf(arena, " ADC_DONE");
    if(fl.flag2.dpdm_done_flag) furi_string_cat_printf(arena, " DPDM_DONE");
    if(furi_string_size(arena) != 0) FURI_LOG_I(TAG, "  IRQ2:    0x%02X %s", fl.data[2], furi_string_get_cstr(arena));

    furi_string_set(arena, "");
    if(fl.flag3.ts_hot_flag) furi_string_cat_printf(arena, " TS_HOT");
    if(fl.flag3.ts_warm_flag) furi_string_cat_printf(arena, " TS_WARM");
    if(fl.flag3.ts_cool_flag) furi_string_cat_printf(arena, " TS_COOL");
    if(fl.flag3.ts_cold_flag) furi_string_cat_printf(arena, " TS_COLD");
    if(fl.flag3.vbatotg_low_flag) furi_string_cat_printf(arena, " VBATOTG_LOW");
    if(furi_string_size(arena) != 0) FURI_LOG_I(TAG, "  IRQ3:    0x%02X %s", fl.data[3], furi_string_get_cstr(arena));
    furi_string_free(arena);
}
#endif

static void __isr __not_in_flash_func(power_bq25792_event_isr)(void* context) {
    Power* instance = (Power*)context;
    furi_event_loop_set_custom_event(instance->event_loop, PowerEventTypeIsr);

    Bq25792ChargerFlagReg fl = {0};
    bq25792_get_charger_irq_flags(instance->bq25792_header, &fl);

    if(fl.flag0.iindpm_flag && instance->otg_enabled) {
        if(instance->otg_overcurrent.callback) instance->otg_overcurrent.callback(instance->otg_overcurrent.context);
    }

#ifdef BQ25792_IRQ_DEBUG_ENABLE
    power_bq25792_print_charger_irq(instance, fl);
#endif
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

/* Wrapper for power-side BQ25792 reset/config function so it can be queued via PowerMessage */
API_WRAPPER(power_bq25792_reset_and_load_config, Bq25792Status, Power*);

typedef struct {
    uint16_t voltage_mv;
    uint16_t current_ma;
} PowerBq25792OtgParams;

static void power_bq25792_otg_watchdog_timer_callback(void* context) {
    furi_assert(context);
    Power* instance = (Power*)context;
    if(bq25792_watchdog_reset(instance->bq25792_header) != Bq25792StatusOk) {
        FURI_LOG_E(TAG, "OTG watchdog reset failed");
    }
}

static Bq25792Status power_bq25792_set_otg_params_internal(Power* instance, PowerBq25792OtgParams* params) {
    furi_assert(instance);
    furi_assert(params);
    Bq25792Status res = bq25792_set_otg_voltage_mv(instance->bq25792_header, params->voltage_mv);
    if(res != Bq25792StatusOk) {
        FURI_LOG_E(TAG, "Failed to set OTG voltage: %d", res);
        return res;
    }
    res = bq25792_set_otg_current_ma(instance->bq25792_header, params->current_ma);
    if(res != Bq25792StatusOk) {
        FURI_LOG_E(TAG, "Failed to set OTG current: %d", res);
    }
    return res;
}

static Bq25792Status power_bq25792_otg_enable_internal(Power* instance, bool enable) {
    furi_assert(instance);
    Bq25792Status res = Bq25792StatusUnknown;

    if(enable) {
        res = bq25792_watchdog_set_time(instance->bq25792_header, BQ25792_OTG_WATCHDOG_TIME);
        if(res != Bq25792StatusOk) {
            FURI_LOG_E(TAG, "Failed to set OTG watchdog time: %d", res);
            return res;
        }
        res = bq25792_watchdog_reset(instance->bq25792_header);
        if(res != Bq25792StatusOk) {
            FURI_LOG_E(TAG, "Failed to reset OTG watchdog: %d", res);
            return res;
        }
        res = bq25792_otg_enable(instance->bq25792_header, true);
        if(res != Bq25792StatusOk) {
            FURI_LOG_E(TAG, "Failed to enable OTG: %d", res);
            return res;
        }
        instance->otg_enabled = true;
        furi_event_loop_timer_start(instance->otg_watchdog_timer, BQ25792_OTG_WATCHDOG_PERIOD_MS);
    } else {
        // If OTG disable fails, stop pets so the chip WD resets it for us;
        // keep WD enabled until OTG is confirmed off.
        res = bq25792_otg_enable(instance->bq25792_header, false);
        furi_event_loop_timer_stop(instance->otg_watchdog_timer);

        if(res != Bq25792StatusOk) {
            FURI_LOG_E(TAG, "Failed to disable OTG: %d", res);
            return res;
        }

        instance->otg_enabled = false;

        res = bq25792_watchdog_set_time(instance->bq25792_header, Bq25792WatchdogTimeDisabled);
        if(res != Bq25792StatusOk) {
            FURI_LOG_E(TAG, "Failed to disable OTG watchdog: %d", res);
        }
    }
    return res;
}

static Bq25792Status power_bq25792_is_usb_connected(Power* instance, bool* usb_connected) {
    furi_assert(instance);
    furi_assert(usb_connected); 
    Bq25792Status res = Bq25792StatusUnknown;

    Bq25792ChargerStatusReg status = {0};
    res = bq25792_get_charger_status(instance->bq25792_header, &status);
    if(res != Bq25792StatusOk) {
        FURI_LOG_E(TAG, "Failed to get charger status: %d", res);
        return res;
    }
    *usb_connected = !!status.stat0.vbus_present_stat;
    return res;
}

API_WRAPPER_PARAM(power_bq25792_set_otg_params_internal, Bq25792Status, Power*, PowerBq25792OtgParams*);
API_WRAPPER_PARAM(power_bq25792_otg_enable_internal, Bq25792Status, Power*, bool);
API_WRAPPER_PARAM(power_bq25792_is_usb_connected, Bq25792Status, Power*, bool*);
// Bq28z620 wrappers

API_WRAPPER_PARAM(bq28z620_get_control_status, Bq28z620Status, Bq28z620*, Bq28z620StdCmdControlStatusRegBits*);
API_WRAPPER_PARAM(bq28z620_get_time_to_empty, Bq28z620Status, Bq28z620*, uint16_t*);
API_WRAPPER_PARAM(bq28z620_get_temperature, Bq28z620Status, Bq28z620*, float*);
API_WRAPPER_PARAM(bq28z620_get_voltage, Bq28z620Status, Bq28z620*, float*);
API_WRAPPER_PARAM(bq28z620_get_battery_status, Bq28z620Status, Bq28z620*, Bq28z620StdCmdBatteryStatusRegBits*);
API_WRAPPER_PARAM(bq28z620_get_current, Bq28z620Status, Bq28z620*, int16_t*);
API_WRAPPER_PARAM(bq28z620_get_remaining_capacity, Bq28z620Status, Bq28z620*, uint16_t*);
API_WRAPPER_PARAM(bq28z620_get_full_charge_capacity, Bq28z620Status, Bq28z620*, uint16_t*);
API_WRAPPER_PARAM(bq28z620_get_average_current, Bq28z620Status, Bq28z620*, int16_t*);
API_WRAPPER_PARAM(bq28z620_get_average_time_to_empty, Bq28z620Status, Bq28z620*, uint16_t*);
API_WRAPPER_PARAM(bq28z620_get_average_time_to_full, Bq28z620Status, Bq28z620*, uint16_t*);
API_WRAPPER_PARAM(bq28z620_get_standby_current, Bq28z620Status, Bq28z620*, int16_t*);
API_WRAPPER_PARAM(bq28z620_get_standby_time_to_empty, Bq28z620Status, Bq28z620*, uint16_t*);
API_WRAPPER_PARAM(bq28z620_get_max_load_current, Bq28z620Status, Bq28z620*, int16_t*);
API_WRAPPER_PARAM(bq28z620_get_max_load_time_to_empty, Bq28z620Status, Bq28z620*, uint16_t*);
API_WRAPPER_PARAM(bq28z620_get_average_power, Bq28z620Status, Bq28z620*, int16_t*);
API_WRAPPER_PARAM(bq28z620_get_internal_temperature, Bq28z620Status, Bq28z620*, float*);
API_WRAPPER_PARAM(bq28z620_get_cycle_count, Bq28z620Status, Bq28z620*, uint16_t*);
API_WRAPPER_PARAM(bq28z620_get_relative_state_of_charge, Bq28z620Status, Bq28z620*, uint8_t*);
API_WRAPPER_PARAM(bq28z620_get_state_of_health, Bq28z620Status, Bq28z620*, uint8_t*);
API_WRAPPER_PARAM(bq28z620_get_charging_voltage, Bq28z620Status, Bq28z620*, float*);
API_WRAPPER_PARAM(bq28z620_get_charging_current, Bq28z620Status, Bq28z620*, int16_t*);
API_WRAPPER_PARAM(bq28z620_get_design_capacity, Bq28z620Status, Bq28z620*, uint16_t*);

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
    instance->devices = 0;
    instance->otg_enabled = false;
    instance->otg_overcurrent = (FuriCallbackWithContext){0};
    instance->otg_watchdog_timer =
        furi_event_loop_timer_alloc(instance->event_loop, power_bq25792_otg_watchdog_timer_callback, FuriEventLoopTimerTypePeriodic, instance);

    // init bq25792
    instance->bq25792_header = bq25792_init(&furi_hal_i2c_handle_main, BQ25792_ADDRESS, NULL);
    if(instance->bq25792_header) {
        instance->devices |= PowerDeviceBq25792;
        /* call synchronous implementation to initialize charger */
        power_bq25792_reset_and_load_config(instance);
        furi_bsp_expander_main_attach_bq25792_callback(power_bq25792_event_isr, instance);

    } else {
        FURI_LOG_E(TAG, "Failed to initialize BQ25792");
    }

    // init ina219
    instance->ina219_header = ina219_init(&furi_hal_i2c_handle_main, INA219_ADDRESS, POWER_INA_SHUNT_RESISTOR_OHMS, POWER_INA_BUS_CURRENT_MAX);
    if(instance->ina219_header) {
        instance->devices |= PowerDeviceIna219;
    } else {
        FURI_LOG_E(TAG, "Failed to initialize INA219");
    }

    // init bq28z620
    instance->bq28z620_header = bq28z620_init(&furi_hal_i2c_handle_main, BQ28Z620_ADDRESS);
    if(instance->bq28z620_header) {
        instance->devices |= PowerDeviceBq28z620;
    } else {
        FURI_LOG_E(TAG, "Failed to initialize BQ28Z620");
    }

    furi_event_loop_subscribe_message_queue(instance->event_loop, instance->message_queue, FuriEventLoopEventIn, power_message_queue_callback, instance);
    furi_event_loop_set_custom_event_callback(instance->event_loop, power_custom_event_callback, instance);

    instance->event_pubsub = furi_pubsub_alloc();
    furi_record_create(RECORD_POWER, instance);

    return instance;
}

bool power_is_device_initialized(Power* instance, PowerDevice* device) {
    furi_check(instance);
    if(device) {
        *device = instance->devices;
    }
    return (instance->devices & PowerDeviceAllInit) == PowerDeviceAllInit;
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

#define POWER_API_CALL_PARAM(dev, fn, ctx, par, res)                             \
    do {                                                                         \
        if(0) {                                                                  \
            /* typechecking */                                                   \
            res = fn(ctx, par);                                                  \
        }                                                                        \
        if(dev) {                                                                \
            /* device checking */                                                \
            if(!((instance)->devices & (dev))) {                                 \
                FURI_LOG_E(TAG, "Device not initialized for API call: %s", #fn); \
                break;                                                           \
            }                                                                    \
        }                                                                        \
        PowerMessage msg = {                                                     \
            .function = fn##_api,                                                \
            .context = ctx,                                                      \
            .param = &(par),                                                     \
            .result = &(res),                                                    \
            .lock = api_lock_alloc_locked(),                                     \
        };                                                                       \
        power_send_message(instance, &msg);                                      \
    } while(0);

#define POWER_API_CALL(dev, fn, ctx, res)                                        \
    do {                                                                         \
        if(0) {                                                                  \
            /* typechecking */                                                   \
            res = fn(ctx);                                                       \
        }                                                                        \
        if(dev) {                                                                \
            /* device checking */                                                \
            if(!((instance)->devices & (dev))) {                                 \
                FURI_LOG_E(TAG, "Device not initialized for API call: %s", #fn); \
                break;                                                           \
            }                                                                    \
        }                                                                        \
        PowerMessage msg = {                                                     \
            .function = fn##_api,                                                \
            .context = ctx,                                                      \
            .param = NULL,                                                       \
            .result = &(res),                                                    \
            .lock = api_lock_alloc_locked(),                                     \
        };                                                                       \
        power_send_message(instance, &msg);                                      \
    } while(0);

// Ina219 API functions

float_t power_ina219_get_voltage_v(Power* instance) {
    furi_check(instance);
    float_t voltage;
    POWER_API_CALL(PowerDeviceIna219, ina219_get_bus_voltage_v, instance->ina219_header, voltage);
    return voltage;
}

float_t power_ina219_get_current_a(Power* instance) {
    furi_check(instance);
    float_t current;
    POWER_API_CALL(PowerDeviceIna219, ina219_get_current_a, instance->ina219_header, current);
    return current;
}

float_t power_ina219_get_power_w(Power* instance) {
    furi_check(instance);
    float_t power;
    POWER_API_CALL(PowerDeviceIna219, ina219_get_power_w, instance->ina219_header, power);
    return power;
}

float_t power_ina219_get_shunt_voltage_mv(Power* instance) {
    furi_check(instance);
    float_t shunt_voltage;
    POWER_API_CALL(PowerDeviceIna219, ina219_get_shunt_voltage_mv, instance->ina219_header, shunt_voltage);
    return shunt_voltage;
}

// Bq25792 API functions

bool power_bq25792_reset_config(Power* instance) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL(PowerDeviceBq25792, power_bq25792_reset_and_load_config, instance, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_set_power_switch(Power* instance, Bq25792PowerSwitch power_switch) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq25792, bq25792_set_power_switch, instance->bq25792_header, power_switch, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_get_ibus_ma(Power* instance, int16_t* ibus) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq25792, bq25792_get_ibus_ma, instance->bq25792_header, ibus, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_get_ibat_ma(Power* instance, int16_t* ibat) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq25792, bq25792_get_ibat_ma, instance->bq25792_header, ibat, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_get_vbus_mv(Power* instance, uint16_t* vbus) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq25792, bq25792_get_vbus_mv, instance->bq25792_header, vbus, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_get_vbat_mv(Power* instance, uint16_t* vbat) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq25792, bq25792_get_vbat_mv, instance->bq25792_header, vbat, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_get_vsys_mv(Power* instance, uint16_t* vsys) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq25792, bq25792_get_vsys_mv, instance->bq25792_header, vsys, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_get_charger_temperature(Power* instance, float* temperature) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq25792, bq25792_get_charger_temperature, instance->bq25792_header, temperature, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_get_temperature_battery_celsius(Power* instance, float* temperature) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq25792, bq25792_get_temperature_battery_celsius, instance->bq25792_header, temperature, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_get_input_current_limit_ma(Power* instance, uint16_t* input_current_limit) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq25792, bq25792_get_input_current_limit_ma, instance->bq25792_header, input_current_limit, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_set_input_current_limit_ma(Power* instance, uint16_t input_current_limit) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq25792, bq25792_set_input_current_limit_ma, instance->bq25792_header, input_current_limit, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_get_charge_voltage_limit_ma(Power* instance, uint16_t* charge_voltage_limit) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq25792, bq25792_get_charge_voltage_limit_ma, instance->bq25792_header, charge_voltage_limit, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_set_charge_voltage_limit_ma(Power* instance, uint16_t charge_voltage_limit) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq25792, bq25792_set_charge_voltage_limit_ma, instance->bq25792_header, charge_voltage_limit, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_get_charge_current_limit_ma(Power* instance, uint16_t* charge_current_limit) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq25792, bq25792_get_charge_current_limit_ma, instance->bq25792_header, charge_current_limit, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_set_charge_current_limit_ma(Power* instance, uint16_t charge_current_limit) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq25792, bq25792_set_charge_current_limit_ma, instance->bq25792_header, charge_current_limit, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_charge_enable(Power* instance, bool enable) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq25792, bq25792_charge_enable, instance->bq25792_header, enable, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_get_charger_status(Power* instance, Bq25792ChargerStatusReg* status) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq25792, bq25792_get_charger_status, instance->bq25792_header, status, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_get_charger_fault(Power* instance, Bq25792FaultStatusReg* fault) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq25792, bq25792_get_charger_fault, instance->bq25792_header, fault, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_get_charger_irq_flags(Power* instance, Bq25792ChargerFlagReg* irq_flags) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq25792, bq25792_get_charger_irq_flags, instance->bq25792_header, irq_flags, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_adc_enable(Power* instance, bool enable) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq25792, bq25792_adc_enable, instance->bq25792_header, enable, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_watchdog_reset(Power* instance) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL(PowerDeviceBq25792, bq25792_watchdog_reset, instance->bq25792_header, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_set_otg_params(Power* instance, uint16_t voltage_mv, uint16_t current_ma) {
    furi_check(instance);
    Bq25792Status result;
    PowerBq25792OtgParams params = {.voltage_mv = voltage_mv, .current_ma = current_ma};
    PowerBq25792OtgParams* params_ptr = &params;
    POWER_API_CALL_PARAM(PowerDeviceBq25792, power_bq25792_set_otg_params_internal, instance, params_ptr, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_otg_enable(Power* instance, bool enable) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq25792, power_bq25792_otg_enable_internal, instance, enable, result);
    return result == Bq25792StatusOk;
}

void power_bq25792_set_otg_overcurrent_callback(Power* instance, FuriCallback callback, void* context) {
    furi_check(instance);
    FURI_CRITICAL_ENTER();
    instance->otg_overcurrent.callback = callback;
    instance->otg_overcurrent.context = context;
    FURI_CRITICAL_EXIT();
}

bool power_bq25792_get_ico_current_limit_ma(Power* instance, uint16_t* ico_current_limit) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq25792, bq25792_get_ico_current_limit_ma, instance->bq25792_header, ico_current_limit, result);
    return result == Bq25792StatusOk;
}

bool power_bq25792_usb_is_connected(Power* instance, bool* usb_connected) {
    furi_check(instance);
    Bq25792Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq25792, power_bq25792_is_usb_connected, instance, usb_connected, result);
    return result == Bq25792StatusOk;
}

// Bq28z620 API functions

bool power_bq28z620_get_control_status(Power* instance, Bq28z620StdCmdControlStatusRegBits* control_status) {
    furi_check(instance);
    Bq28z620Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq28z620, bq28z620_get_control_status, instance->bq28z620_header, control_status, result);
    return result == Bq28z620StatusOk;
}

bool power_bq28z620_get_time_to_empty(Power* instance, uint16_t* time_to_empty) {
    furi_check(instance);
    Bq28z620Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq28z620, bq28z620_get_time_to_empty, instance->bq28z620_header, time_to_empty, result);
    return result == Bq28z620StatusOk;
}

bool power_bq28z620_get_temperature(Power* instance, float* temperature) {
    furi_check(instance);
    Bq28z620Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq28z620, bq28z620_get_temperature, instance->bq28z620_header, temperature, result);
    return result == Bq28z620StatusOk;
}

bool power_bq28z620_get_voltage(Power* instance, float* voltage) {
    furi_check(instance);
    Bq28z620Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq28z620, bq28z620_get_voltage, instance->bq28z620_header, voltage, result);
    return result == Bq28z620StatusOk;
}

bool power_bq28z620_get_battery_status(Power* instance, Bq28z620StdCmdBatteryStatusRegBits* battery_status) {
    furi_check(instance);
    Bq28z620Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq28z620, bq28z620_get_battery_status, instance->bq28z620_header, battery_status, result);
    return result == Bq28z620StatusOk;
}

bool power_bq28z620_get_current(Power* instance, int16_t* current) {
    furi_check(instance);
    Bq28z620Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq28z620, bq28z620_get_current, instance->bq28z620_header, current, result);
    return result == Bq28z620StatusOk;
}

bool power_bq28z620_get_remaining_capacity(Power* instance, uint16_t* remaining_capacity) {
    furi_check(instance);
    Bq28z620Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq28z620, bq28z620_get_remaining_capacity, instance->bq28z620_header, remaining_capacity, result);
    return result == Bq28z620StatusOk;
}

bool power_bq28z620_get_full_charge_capacity(Power* instance, uint16_t* full_charge_capacity) {
    furi_check(instance);
    Bq28z620Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq28z620, bq28z620_get_full_charge_capacity, instance->bq28z620_header, full_charge_capacity, result);
    return result == Bq28z620StatusOk;
}

bool power_bq28z620_get_average_current(Power* instance, int16_t* average_current) {
    furi_check(instance);
    Bq28z620Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq28z620, bq28z620_get_average_current, instance->bq28z620_header, average_current, result);
    return result == Bq28z620StatusOk;
}

bool power_bq28z620_get_average_time_to_empty(Power* instance, uint16_t* average_time_to_empty) {
    furi_check(instance);
    Bq28z620Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq28z620, bq28z620_get_average_time_to_empty, instance->bq28z620_header, average_time_to_empty, result);
    return result == Bq28z620StatusOk;
}

bool power_bq28z620_get_average_time_to_full(Power* instance, uint16_t* average_time_to_full) {
    furi_check(instance);
    Bq28z620Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq28z620, bq28z620_get_average_time_to_full, instance->bq28z620_header, average_time_to_full, result);
    return result == Bq28z620StatusOk;
}

bool power_bq28z620_get_standby_current(Power* instance, int16_t* standby_current) {
    furi_check(instance);
    Bq28z620Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq28z620, bq28z620_get_standby_current, instance->bq28z620_header, standby_current, result);
    return result == Bq28z620StatusOk;
}

bool power_bq28z620_get_standby_time_to_empty(Power* instance, uint16_t* standby_time_to_empty) {
    furi_check(instance);
    Bq28z620Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq28z620, bq28z620_get_standby_time_to_empty, instance->bq28z620_header, standby_time_to_empty, result);
    return result == Bq28z620StatusOk;
}

bool power_bq28z620_get_max_load_current(Power* instance, int16_t* max_load_current) {
    furi_check(instance);
    Bq28z620Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq28z620, bq28z620_get_max_load_current, instance->bq28z620_header, max_load_current, result);
    return result == Bq28z620StatusOk;
}

bool power_bq28z620_get_max_load_time_to_empty(Power* instance, uint16_t* max_load_time_to_empty) {
    furi_check(instance);
    Bq28z620Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq28z620, bq28z620_get_max_load_time_to_empty, instance->bq28z620_header, max_load_time_to_empty, result);
    return result == Bq28z620StatusOk;
}

bool power_bq28z620_get_average_power(Power* instance, int16_t* average_power) {
    furi_check(instance);
    Bq28z620Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq28z620, bq28z620_get_average_power, instance->bq28z620_header, average_power, result);
    return result == Bq28z620StatusOk;
}

bool power_bq28z620_get_internal_temperature(Power* instance, float* internal_temperature) {
    furi_check(instance);
    Bq28z620Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq28z620, bq28z620_get_internal_temperature, instance->bq28z620_header, internal_temperature, result);
    return result == Bq28z620StatusOk;
}

bool power_bq28z620_get_cycle_count(Power* instance, uint16_t* cycle_count) {
    furi_check(instance);
    Bq28z620Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq28z620, bq28z620_get_cycle_count, instance->bq28z620_header, cycle_count, result);
    return result == Bq28z620StatusOk;
}

bool power_bq28z620_get_relative_state_of_charge(Power* instance, uint8_t* relative_state_of_charge) {
    furi_check(instance);
    Bq28z620Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq28z620, bq28z620_get_relative_state_of_charge, instance->bq28z620_header, relative_state_of_charge, result);
    return result == Bq28z620StatusOk;
}

bool power_bq28z620_get_state_of_health(Power* instance, uint8_t* state_of_health) {
    furi_check(instance);
    Bq28z620Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq28z620, bq28z620_get_state_of_health, instance->bq28z620_header, state_of_health, result);
    return result == Bq28z620StatusOk;
}

bool power_bq28z620_get_charging_voltage(Power* instance, float* charging_voltage) {
    furi_check(instance);
    Bq28z620Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq28z620, bq28z620_get_charging_voltage, instance->bq28z620_header, charging_voltage, result);
    return result == Bq28z620StatusOk;
}

bool power_bq28z620_get_charging_current(Power* instance, int16_t* charging_current) {
    furi_check(instance);
    Bq28z620Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq28z620, bq28z620_get_charging_current, instance->bq28z620_header, charging_current, result);
    return result == Bq28z620StatusOk;
}

bool power_bq28z620_get_design_capacity(Power* instance, uint16_t* design_capacity) {
    furi_check(instance);
    Bq28z620Status result;
    POWER_API_CALL_PARAM(PowerDeviceBq28z620, bq28z620_get_design_capacity, instance->bq28z620_header, design_capacity, result);
    return result == Bq28z620StatusOk;
}
