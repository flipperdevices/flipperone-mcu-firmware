// USB-C Power Delivery service.
//
// Owns the PD/UCSI stack (lib/usb_v2) and everything product-specific about
// it: the power path (bq25792 OTG through the power service) and the Type-C
// policy below. The stack runs on its own thread, so this service's event
// loop exists for the request API — callers post a message and, when they
// need the result, wait on an api_lock.
//
// Exposing UCSI to an external OPM is NOT done here: this service only hands
// out the UsbPd handle via pd_get_usb_pd(), and the transport (i2c_negotiator)
// maps it into the intercom register space.

#include "pd.h"

#include <furi.h>
#include <api_lock.h>
#include <furi_hal_resources.h>
#include <power/power.h>

#define TAG "Pd"

#define PD_MAX_MESSAGES (8)

// Floor applied when the port grants nothing (detached, or sourcing). Low
// enough to be safe on any USB port, high enough to keep charging alive.
#define PD_SINK_MIN_INPUT_MA (100u)

struct Pd {
    FuriEventLoop* event_loop;
    FuriMessageQueue* message_queue;

    Power* power;
    // NULL if the FUSB302 failed to come up. The service and its record
    // exist either way, so callers get an honest answer instead of blocking
    // forever on furi_record_open.
    UsbPd* usb_pd;

    FuriCallback ucsi_alert_callback;
    void* ucsi_alert_context;
};

typedef enum {
    PdMessageTypeResetConfig,
    PdMessageTypeSetInputLimit,
} PdMessageType;

typedef struct {
    PdMessageType type;
    FuriApiLock lock;
    bool* result;
    uint16_t input_limit_ma;
    bool probe_allowed;
} PdMessage;

// --- UsbPd callbacks (UsbPd worker thread) ---------------------------------

static void pd_vbus_source_set(void* context, bool enable) {
    Pd* instance = context;
    if(enable) {
        // Start at vSafe5V; the partner's Request retunes us via pd_power_supply_set.
        if(!power_bq25792_set_otg_params(instance->power, 5000, 1500)) {
            FURI_LOG_W(TAG, "otg params 5V/1.5A failed");
        }
        if(!power_bq25792_otg_enable(instance->power, true)) {
            FURI_LOG_W(TAG, "otg enable failed");
        }
    } else {
        (void)power_bq25792_otg_enable(instance->power, false);
    }
}

static bool pd_power_supply_set(void* context, uint16_t voltage_mv, uint16_t current_limit_ma) {
    Pd* instance = context;
    if(!power_bq25792_set_otg_params(instance->power, voltage_mv, current_limit_ma)) {
        FURI_LOG_W(TAG, "otg params %umV/%umA failed", voltage_mv, current_limit_ma);
        return false;
    }
    return true;
}

// Applies what the port allows us to draw to the charger's input limit.
//
// This closes the failure that dominated bring-up: bq25792 boots configured
// for its own maximum and starts optimizing input current the moment VBUS
// appears, roughly 190 ms in. PD negotiation lands in the same window, so
// whichever won the race decided the outcome — when the charger won it
// pulled the source below its Type-C advertisement, the BMC frames on CC
// arrived corrupted, and the source Hard Reset the connection.
static void pd_sink_current_limit_set(void* context, uint16_t current_ma, UcsiPpmSinkLimitSource source) {
    Pd* instance = context;

    // Input Current Optimization deliberately draws past the configured limit
    // until the source sags, to find what it can really deliver. That is only
    // acceptable once nobody is negotiating any more: a partner still in the
    // middle of PD sees the overdraw as a fault and Hard Resets the link, and
    // ICO starts the moment VBUS appears — long before a contract exists.
    const bool probe_allowed = source == UcsiPpmSinkLimitTypeCOnly;
    static const char* const source_names[] = {"none", "type-c Rp", "type-c only", "pd contract"};
    FURI_LOG_I(
        TAG,
        "input limit: %u mA (%s, ico %s)",
        current_ma,
        source_names[source],
        probe_allowed ? "on" : "off");

    // Runs on the PD worker thread, in the middle of PD event handling.
    // power_bq25792_set_input_current_limit_ma() blocks on the power service
    // thread and then talks I2C on the same bus the FUSB302 is on, so calling
    // it from here stalls PD exactly when frames need servicing. Hand it to
    // our own thread instead and return immediately.
    const PdMessage msg = {
        .type = PdMessageTypeSetInputLimit,
        // Never leave the charger above what the port grants;
        // PD_SINK_MIN_INPUT_MA keeps it alive when nothing is granted at all.
        .input_limit_ma = current_ma > 0u ? current_ma : PD_SINK_MIN_INPUT_MA,
        .probe_allowed = probe_allowed,
    };
    if(furi_message_queue_put(instance->message_queue, &msg, 0) != FuriStatusOk) {
        FURI_LOG_W(TAG, "input limit %u mA dropped, queue full", msg.input_limit_ma);
    }
}

static void pd_ucsi_alert(void* context) {
    Pd* instance = context;

    // Snapshot the pair so a concurrent pd_set_ucsi_alert_callback cannot
    // pair a new callback with a stale context.
    FURI_CRITICAL_ENTER();
    const FuriCallback callback = instance->ucsi_alert_callback;
    void* callback_context = instance->ucsi_alert_context;
    FURI_CRITICAL_EXIT();

    if(callback) {
        callback(callback_context);
    }
}

// --- observability ---------------------------------------------------------

static const char* pd_connector_state_str(UcsiPpmConnectorState state) {
    switch(state) {
    case UcsiPpmStateUnattached:
        return "Unattached";
    case UcsiPpmStateAttachWait:
        return "AttachWait";
    case UcsiPpmStateAttachedSrc:
        return "Attached.SRC";
    case UcsiPpmStateAttachedSnk:
        return "Attached.SNK";
    case UcsiPpmStateErrorRecovery:
        return "ErrorRecovery";
    case UcsiPpmStateDisabled:
        return "Disabled";
    }
    return "?";
}

// Runs on the UsbPd worker thread. The PD stack itself is silent, so this is
// the only place the connector's behaviour shows up in the log — keep it,
// bring-up on the host side is unreadable without it.
static void pd_usb_pd_event(const void* message, void* context) {
    UNUSED(context);
    const UsbPdEvent* event = message;

    switch(event->type) {
    case UsbPdEventTypeConnectorStateChanged:
        FURI_LOG_I(TAG, "connector: %s", pd_connector_state_str(event->connector_state));
        break;
    case UsbPdEventTypeContractChanged:
        if(event->contract.contract_in_place) {
            FURI_LOG_I(
                TAG,
                "contract: %u mV %u mA, we are %s/%s",
                event->contract.voltage_mv,
                event->contract.current_ma,
                event->contract.is_source ? "source" : "sink",
                event->contract.is_dfp ? "DFP" : "UFP");
        } else {
            FURI_LOG_I(TAG, "contract: dropped");
        }
        break;
    }
}

// --- service thread --------------------------------------------------------

static bool pd_handle_reset_config(Pd* instance) {
    if(!instance->usb_pd) return false;
    usb_pd_reset(instance->usb_pd);
    return true;
}

static void pd_message_queue_callback(FuriEventLoopObject* object, void* context) {
    Pd* instance = context;
    furi_assert(object == instance->message_queue);

    PdMessage msg;
    furi_check(furi_message_queue_get(instance->message_queue, &msg, 0) == FuriStatusOk);

    bool result = false;
    switch(msg.type) {
    case PdMessageTypeResetConfig:
        result = pd_handle_reset_config(instance);
        break;
    case PdMessageTypeSetInputLimit:
        // Order matters on the way up: stop the optimizer before raising the
        // ceiling, so it cannot be mid-probe with the new number.
        if(!msg.probe_allowed && !power_bq25792_ico_enable(instance->power, false)) {
            FURI_LOG_W(TAG, "ico disable failed");
        }
        result = power_bq25792_set_input_current_limit_ma(instance->power, msg.input_limit_ma);
        if(!result) {
            FURI_LOG_W(TAG, "input limit %u mA failed", msg.input_limit_ma);
        }
        if(msg.probe_allowed && !power_bq25792_ico_enable(instance->power, true)) {
            FURI_LOG_W(TAG, "ico enable failed");
        }
        // What we asked for and what the charger ended up with are not the
        // same question: several of its own mechanisms rewrite the input
        // current limit behind us, and VINDPM — the voltage it is willing to
        // drag the source down to before backing off — resets itself to
        // 3600 mV on every unplug. Read both back rather than assume.
        {
            uint16_t applied_ma = 0;
            uint16_t vindpm_mv = 0;
            if(power_bq25792_get_input_current_limit_ma(instance->power, &applied_ma) &&
               power_bq25792_get_input_voltage_limit_mv(instance->power, &vindpm_mv)) {
                FURI_LOG_I(TAG, "charger now: iindpm=%u mA vindpm=%u mV", applied_ma, vindpm_mv);
            }
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

static void pd_send_message(Pd* instance, const PdMessage* message) {
    furi_check(furi_message_queue_put(instance->message_queue, message, FuriWaitForever) == FuriStatusOk);
    if(message->lock) {
        api_lock_wait_unlock_and_free(message->lock);
    }
}

// --- policy ----------------------------------------------------------------

// Everything the PD stack cannot know about the product: what we are willing
// to source, what we are willing to sink, and how the connector behaves.
// Policy is spelled out here in full even where it matches the library
// default — this is the one place that decides how the port behaves, and it
// should not silently change if a default in lib/usb_v2 ever does.
static void pd_config_fill(Pd* instance, UsbPdConfig* config) {
    usb_pd_config_init_default(config);

    config->callback_context = instance;
    config->vbus_source_set = pd_vbus_source_set;
    config->power_supply_set = pd_power_supply_set;
    config->sink_current_limit_set = pd_sink_current_limit_set;
    config->ucsi_alert = pd_ucsi_alert;

    // FUSB302 INT_N is wired both into the main expander and straight to this
    // pin. We take the direct line: no I2C round trip through the expander to
    // find out who interrupted, and no dependency on the expander's own
    // interrupt configuration. The pin is named after the headset interrupt
    // it was originally meant for and is otherwise unused.
    config->irq_gpio = &gpio_cpu_audio_hp_int;

    config->cc_operation_mode = UcsiPpmCcModeDrp;
    config->source_rp_current = UcsiPpmRpCurrent1A5;

    // Source: vSafe5V is mandatory at position 1, the rest are upgrades the
    // partner may Request. Bounded by what bq25792 OTG can deliver.
    config->source_caps.pdos[0] = ucsi_ppm_pdo_fixed_source(5000, 3000, true, false, true, true);
    config->source_caps.pdos[1] = ucsi_ppm_pdo_fixed_source(9000, 3000, true, false, true, true);
    config->source_caps.pdos[2] = ucsi_ppm_pdo_fixed_source(12000, 3000, true, false, true, true);
    config->source_caps.pdos[3] = ucsi_ppm_pdo_fixed_source(15000, 3000, true, false, true, true);
    config->source_caps.count = 4;

    config->sink_caps.pdos[0] = ucsi_ppm_pdo_fixed_sink(5000, 3000, true, false, false, true, true);
    config->sink_caps.pdos[1] = ucsi_ppm_pdo_fixed_sink(9000, 3000, true, false, false, true, true);
    config->sink_caps.pdos[2] = ucsi_ppm_pdo_fixed_sink(12000, 3000, true, false, false, true, true);
    config->sink_caps.pdos[3] = ucsi_ppm_pdo_fixed_sink(15000, 3000, true, false, false, true, true);
    config->sink_caps.count = 4;

    config->supports_usb_pd = true;
    config->power_source_vbus = true;
    config->connector_usb2_capable = true;
}

static Pd* pd_alloc(void) {
    Pd* instance = malloc(sizeof(Pd));

    instance->event_loop = furi_event_loop_alloc();
    instance->message_queue = furi_message_queue_alloc(PD_MAX_MESSAGES, sizeof(PdMessage));
    furi_event_loop_subscribe_message_queue(
        instance->event_loop, instance->message_queue, FuriEventLoopEventIn, pd_message_queue_callback, instance);

    instance->power = furi_record_open(RECORD_POWER);

    UsbPdConfig config;
    pd_config_fill(instance, &config);

    instance->usb_pd = usb_pd_alloc(&config);
    if(instance->usb_pd) {
        // Never unsubscribed — the service lives for the lifetime of the system.
        furi_pubsub_subscribe(usb_pd_get_pubsub(instance->usb_pd), pd_usb_pd_event, instance);
    } else {
        FURI_LOG_E(TAG, "Failed to initialize PD stack");
    }

    furi_record_create(RECORD_PD, instance);

    return instance;
}

int32_t pd_srv(void* p) {
    UNUSED(p);

    Pd* instance = pd_alloc();
    furi_event_loop_run(instance->event_loop);

    return 0;
}

// --- public API ------------------------------------------------------------

bool pd_is_device_initialized(Pd* instance, PdDevice* device) {
    furi_check(instance);

    const bool initialized = instance->usb_pd != NULL;
    if(device) {
        *device = initialized ? PdDeviceFusb302 : 0;
    }
    if(!initialized) {
        FURI_LOG_E(TAG, "PD device not initialized");
    }
    return initialized;
}

bool pd_reset_config(Pd* instance) {
    furi_check(instance);

    bool result = false;
    PdMessage msg = {
        .type = PdMessageTypeResetConfig,
        .lock = api_lock_alloc_locked(),
        .result = &result,
    };

    pd_send_message(instance, &msg);
    return result;
}

UsbPd* pd_get_usb_pd(Pd* instance) {
    furi_check(instance);
    return instance->usb_pd;
}

void pd_set_ucsi_alert_callback(Pd* instance, FuriCallback callback, void* context) {
    furi_check(instance);

    // Read on the UsbPd worker thread, so publish the pair atomically.
    FURI_CRITICAL_ENTER();
    instance->ucsi_alert_callback = callback;
    instance->ucsi_alert_context = context;
    FURI_CRITICAL_EXIT();
}

FuriPubSub* pd_get_pubsub(Pd* instance) {
    furi_check(instance);
    return instance->usb_pd ? usb_pd_get_pubsub(instance->usb_pd) : NULL;
}
