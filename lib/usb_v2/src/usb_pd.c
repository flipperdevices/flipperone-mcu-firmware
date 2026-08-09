#include "usb_pd.h"

#include <furi.h>
#include <furi_hal_i2c.h>
#include <furi_hal_i2c_config.h>
#include <furi_bsp.h>

#include <stdio.h>
#include <string.h>

#define TAG "UsbPd"

#define USB_PD_DEFAULT_FUSB302_ADDR 0x22u
#define USB_PD_DEFAULT_PHY_POLL_MS  250u
#define USB_PD_DEFAULT_STACK_SIZE   2048u
#define USB_PD_LOG_LINE_MAX         160u

/* Custom event bits delivered to the worker event loop. */
typedef enum {
    UsbPdLoopEventPhyIrq = (1u << 0),
    UsbPdLoopEventPowerReady = (1u << 1),
    UsbPdLoopEventUcsiControl = (1u << 2),
    UsbPdLoopEventReset = (1u << 3),
    UsbPdLoopEventStop = (1u << 4),
} UsbPdLoopEvent;

/* Init handshake bits on init_flag. */
#define USB_PD_INIT_FLAG_DONE (1u << 0)

struct UsbPd {
    UsbPdConfig config; /* Copy with defaults resolved. */

    FuriThread* thread;
    FuriEventFlag* init_flag;
    bool init_ok;

    /* Owned by the worker thread; other threads may only pass it to
     * furi_event_loop_set_custom_event (ISR/cross-thread safe). */
    FuriEventLoop* event_loop;
    /* One-shot timer armed to the next state-machine deadline reported by
     * ucsi_ppm_next_timeout_ms; re-armed in the worker epilogue. */
    FuriEventLoopTimer* sm_timer;
    FuriEventLoopTimer* phy_poll_timer;

    UcsiPpm* ppm;

    /* OPM-facing image of the register file, laid out 1:1 with the UCSI
     * spec. Which side writes a byte follows field ownership, and the two
     * sets never overlap:
     *  - PPM-owned (VERSION, RESERVED1, CCI, MESSAGE_IN) — refreshed from
     *    the core by the worker epilogue;
     *  - OPM-owned (CONTROL, MESSAGE_OUT) — written by usb_pd_ucsi_write
     *    from any context, handed to the core on the doorbell.
     * So one buffer serves both directions and a read is a single memcpy.
     * Note the core zeroes MESSAGE_OUT on PPM_RESET and that is not
     * reflected here — harmless, the OPM owns that field and rewrites it
     * before every command. */
    uint8_t ucsi_regfile[USB_PD_UCSI_REGFILE_SIZE];
    /* Worker-only bounce buffer: MESSAGE_OUT is snapshotted here under a
     * critical section, then pushed into the core outside of it. */
    uint8_t ucsi_staging[USB_PD_UCSI_SIZE_MESSAGE_OUT];

    /* Set by the PPM alert callback, consumed in the worker epilogue. */
    bool alert_pending;

    /* Snapshots guarded by critical sections for cross-thread getters. */
    UcsiPpmConnectorState state_snapshot;
    UcsiPpmContractInfo contract_snapshot;

    FuriPubSub* pubsub;
};

static void usb_pd_worker_ucsi_control(UsbPd* instance);

/* Runs on the BSP expander worker thread (not ISR context). The detach in
 * the worker teardown waits for an in-flight invocation, so `instance` is
 * always valid here. */
static void usb_pd_fusb302_callback(void* context) {
    UsbPd* instance = context;
    furi_event_loop_set_custom_event(instance->event_loop, UsbPdLoopEventPhyIrq);
}

/* --- UcsiPpm HAL adapters (worker thread context) ------------------------ */

static uint32_t usb_pd_hal_time_ms(void* context) {
    UNUSED(context);
    return furi_get_tick();
}

static void usb_pd_hal_alert(void* context) {
    UsbPd* instance = context;
    instance->alert_pending = true;
}

static UcsiPpmStatus usb_pd_hal_i2c_write(void* context, uint8_t addr, const uint8_t* data, size_t len) {
    UsbPd* instance = context;
    furi_hal_i2c_acquire(instance->config.i2c_bus);
    const int rc = furi_hal_i2c_master_tx_blocking(instance->config.i2c_bus, addr, data, len, FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(instance->config.i2c_bus);
    if(rc < 0) {
        FURI_LOG_W(TAG, "i2c write failed: addr=0x%02X len=%u rc=%d", addr, (unsigned)len, rc);
        return UcsiPpmStatusHalError;
    }
    return UcsiPpmStatusOk;
}

static UcsiPpmStatus usb_pd_hal_i2c_read(void* context, uint8_t addr, uint8_t* data, size_t len) {
    UsbPd* instance = context;
    furi_hal_i2c_acquire(instance->config.i2c_bus);
    const int rc = furi_hal_i2c_master_rx_blocking(instance->config.i2c_bus, addr, data, len, FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(instance->config.i2c_bus);
    if(rc < 0) {
        FURI_LOG_W(TAG, "i2c read failed: addr=0x%02X len=%u rc=%d", addr, (unsigned)len, rc);
        return UcsiPpmStatusHalError;
    }
    return UcsiPpmStatusOk;
}

static UcsiPpmStatus usb_pd_hal_i2c_write_read(void* context, uint8_t addr, const uint8_t* tx, size_t tx_len, uint8_t* rx, size_t rx_len) {
    UsbPd* instance = context;
    furi_hal_i2c_acquire(instance->config.i2c_bus);
    const int rc = furi_hal_i2c_master_trx_blocking(instance->config.i2c_bus, addr, tx, tx_len, rx, rx_len, FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(instance->config.i2c_bus);
    if(rc < 0) {
        FURI_LOG_W(TAG, "i2c trx failed: addr=0x%02X tx=%u rx=%u rc=%d", addr, (unsigned)tx_len, (unsigned)rx_len, rc);
        return UcsiPpmStatusHalError;
    }
    return UcsiPpmStatusOk;
}

static void usb_pd_hal_vbus_source(void* context, bool enable) {
    UsbPd* instance = context;
    instance->config.vbus_source_set(instance->config.callback_context, enable);
}

static UcsiPpmStatus usb_pd_hal_power_supply_set(void* context, uint16_t voltage_mv, uint16_t current_limit_ma) {
    UsbPd* instance = context;
    if(!instance->config.power_supply_set(instance->config.callback_context, voltage_mv, current_limit_ma)) {
        return UcsiPpmStatusHalError;
    }
    if(!instance->config.power_supply_ready_async) {
        /* Rail settles synchronously — let the PE emit PS_RDY right away. */
        ucsi_ppm_notify_power_supply_ready(instance->ppm);
    }
    return UcsiPpmStatusOk;
}

static bool usb_pd_hal_has_alt_power(void* context) {
    UsbPd* instance = context;
    if(!instance->config.has_alt_power) return false;
    return instance->config.has_alt_power(instance->config.callback_context);
}

static void usb_pd_hal_log(void* context, UcsiPpmLogLevel level, const char* module, const char* fmt, va_list args) {
    UNUSED(context);
    char buf[USB_PD_LOG_LINE_MAX];
    vsnprintf(buf, sizeof(buf), fmt, args);
    switch(level) {
    case UcsiPpmLogLevelError:
        FURI_LOG_E(module, "%s", buf);
        break;
    case UcsiPpmLogLevelWarn:
        FURI_LOG_W(module, "%s", buf);
        break;
    case UcsiPpmLogLevelInfo:
        FURI_LOG_I(module, "%s", buf);
        break;
    case UcsiPpmLogLevelDebug:
        FURI_LOG_D(module, "%s", buf);
        break;
    case UcsiPpmLogLevelTrace:
        FURI_LOG_T(module, "%s", buf);
        break;
    }
}

static void usb_pd_fill_ppm_config(UsbPd* instance, UcsiPpmConfig* out) {
    const UsbPdConfig* cfg = &instance->config;
    memset(out, 0, sizeof(*out));

    out->hal_ctx = instance;
    out->time_ms = usb_pd_hal_time_ms;
    out->alert = usb_pd_hal_alert;
    out->i2c_read = usb_pd_hal_i2c_read;
    out->i2c_write = usb_pd_hal_i2c_write;
    out->i2c_write_read = usb_pd_hal_i2c_write_read;
    out->gpio_write_vbus_source = usb_pd_hal_vbus_source;
    out->power_supply_set = usb_pd_hal_power_supply_set;
    out->has_alt_power = usb_pd_hal_has_alt_power;
    out->log = usb_pd_hal_log;
    out->fusb302_i2c_addr = cfg->fusb302_i2c_addr;

    out->initial_cc_operation_mode = cfg->cc_operation_mode;
    out->drp_advertise_first = cfg->drp_advertise_first;
    out->source_rp_current = cfg->source_rp_current;
    out->source_caps = cfg->source_caps;
    out->sink_caps = cfg->sink_caps;

    out->supports_disabled_state = cfg->supports_disabled_state;
    out->supports_battery_charging = cfg->supports_battery_charging;
    out->supports_usb_pd = cfg->supports_usb_pd;
    out->supports_typec_current = cfg->supports_typec_current;
    out->power_source_ac = cfg->power_source_ac;
    out->power_source_other = cfg->power_source_other;
    out->power_source_vbus = cfg->power_source_vbus;

    out->supports_set_ccom = cfg->supports_set_ccom;
    out->supports_alt_mode_details = cfg->supports_alt_mode_details;
    out->supports_alt_mode_override = cfg->supports_alt_mode_override;
    out->supports_pdo_details = cfg->supports_pdo_details;
    out->supports_cable_details = cfg->supports_cable_details;
    out->supports_external_supply_notif = cfg->supports_external_supply_notif;
    out->supports_pd_reset_notif = cfg->supports_pd_reset_notif;
    out->supports_get_pd_message = cfg->supports_get_pd_message;
    out->supports_get_attention_vdo = cfg->supports_get_attention_vdo;
    out->supports_fw_update_request = cfg->supports_fw_update_request;
    out->supports_negotiated_pl_notif = cfg->supports_negotiated_pl_notif;
    out->supports_security_request = cfg->supports_security_request;
    out->supports_set_retimer_mode = cfg->supports_set_retimer_mode;
    out->supports_chunking = cfg->supports_chunking;

    out->connector_usb2_capable = cfg->connector_usb2_capable;
    out->connector_usb3_capable = cfg->connector_usb3_capable;
}

/* --- Worker internals ----------------------------------------------------- */

/* Runs after every piece of PPM activity (tick, IRQ pump, UCSI write):
 * refreshes the OPM-facing register file image, delivers the pending UCSI
 * alert, publishes state/contract changes and re-arms the deadline timer. */
static void usb_pd_worker_epilogue(UsbPd* instance) {
    /* Refresh the PPM-owned fields only — CONTROL and MESSAGE_OUT belong to
     * the OPM and would be clobbered mid-write otherwise. */
    FURI_CRITICAL_ENTER();
    ucsi_ppm_register_read(instance->ppm, 0, USB_PD_UCSI_OFFSET_CONTROL, instance->ucsi_regfile);
    ucsi_ppm_register_read(
        instance->ppm,
        USB_PD_UCSI_OFFSET_MESSAGE_IN,
        USB_PD_UCSI_SIZE_MESSAGE_IN,
        &instance->ucsi_regfile[USB_PD_UCSI_OFFSET_MESSAGE_IN]);
    FURI_CRITICAL_EXIT();

    /* Alert strictly after the image refresh, so the OPM glue can serve a
     * coherent CCI read from within the callback. */
    if(instance->alert_pending) {
        instance->alert_pending = false;
        if(instance->config.ucsi_alert) {
            instance->config.ucsi_alert(instance->config.callback_context);
        }
    }

    const UcsiPpmConnectorState state = ucsi_ppm_get_connector_state(instance->ppm);
    UcsiPpmContractInfo contract;
    if(ucsi_ppm_get_contract(instance->ppm, &contract) != UcsiPpmStatusOk) {
        memset(&contract, 0, sizeof(contract));
    }

    const bool state_changed = state != instance->state_snapshot;
    const bool contract_changed = memcmp(&contract, &instance->contract_snapshot, sizeof(contract)) != 0;

    if(state_changed || contract_changed) {
        FURI_CRITICAL_ENTER();
        instance->state_snapshot = state;
        instance->contract_snapshot = contract;
        FURI_CRITICAL_EXIT();
    }

    if(state_changed) {
        const UsbPdEvent event = {
            .type = UsbPdEventTypeConnectorStateChanged,
            .connector_state = state,
            .contract = contract,
        };
        furi_pubsub_publish(instance->pubsub, (void*)&event);
    }
    if(contract_changed) {
        const UsbPdEvent event = {
            .type = UsbPdEventTypeContractChanged,
            .connector_state = state,
            .contract = contract,
        };
        furi_pubsub_publish(instance->pubsub, (void*)&event);
    }

    /* Sleep exactly until the core's next deadline. Any activity above may
     * have armed, re-armed or cancelled a state-machine timer, so this is
     * recomputed on every pass. */
    const uint32_t next_ms = ucsi_ppm_next_timeout_ms(instance->ppm);
    if(next_ms == UCSI_PPM_NO_TIMEOUT) {
        furi_event_loop_timer_stop(instance->sm_timer);
    } else {
        furi_event_loop_timer_start(instance->sm_timer, next_ms ? next_ms : 1u);
    }
}

static void usb_pd_worker_sm_timer_callback(void* context) {
    UsbPd* instance = context;
    ucsi_ppm_tick(instance->ppm);
    usb_pd_worker_epilogue(instance);
}

static void usb_pd_worker_phy_poll_callback(void* context) {
    UsbPd* instance = context;
    /* Lost-IRQ safety net: force an interrupt register pump. The PHY layer
     * only emits events on real changes, so a quiet chip stays quiet. */
    ucsi_ppm_notify_fusb302_irq(instance->ppm);
    ucsi_ppm_tick(instance->ppm);
    usb_pd_worker_epilogue(instance);
}

static void usb_pd_worker_custom_event_callback(uint32_t events, void* context) {
    UsbPd* instance = context;

    if(events & UsbPdLoopEventStop) {
        furi_event_loop_stop(instance->event_loop);
        return;
    }

    if(events & UsbPdLoopEventPhyIrq) {
        ucsi_ppm_notify_fusb302_irq(instance->ppm);
    }
    if(events & UsbPdLoopEventPowerReady) {
        ucsi_ppm_notify_power_supply_ready(instance->ppm);
    }
    if(events & UsbPdLoopEventReset) {
        const UcsiPpmStatus status = ucsi_ppm_reset(instance->ppm);
        if(status != UcsiPpmStatusOk) {
            FURI_LOG_E(TAG, "reset failed: %d", (int)status);
        }
    }
    if(events & UsbPdLoopEventUcsiControl) {
        usb_pd_worker_ucsi_control(instance);
    }
    ucsi_ppm_tick(instance->ppm);
    usb_pd_worker_epilogue(instance);
}

/* UCSI doorbell: the OPM has finished writing CONTROL. Push the staged image
 * into the core — MESSAGE_OUT first so command parameters are in place when
 * the CONTROL write triggers dispatch. */
static void usb_pd_worker_ucsi_control(UsbPd* instance) {
    uint8_t control[USB_PD_UCSI_SIZE_CONTROL];

    FURI_CRITICAL_ENTER();
    memcpy(control, &instance->ucsi_regfile[USB_PD_UCSI_OFFSET_CONTROL], sizeof(control));
    memcpy(instance->ucsi_staging, &instance->ucsi_regfile[USB_PD_UCSI_OFFSET_MESSAGE_OUT], USB_PD_UCSI_SIZE_MESSAGE_OUT);
    FURI_CRITICAL_EXIT();

    UcsiPpmStatus status = ucsi_ppm_register_write(instance->ppm, USB_PD_UCSI_OFFSET_MESSAGE_OUT, USB_PD_UCSI_SIZE_MESSAGE_OUT, instance->ucsi_staging);
    if(status != UcsiPpmStatusOk) {
        FURI_LOG_W(TAG, "ucsi MESSAGE_OUT write rejected: %d", (int)status);
    }
    status = ucsi_ppm_register_write(instance->ppm, USB_PD_UCSI_OFFSET_CONTROL, USB_PD_UCSI_SIZE_CONTROL, control);
    if(status != UcsiPpmStatusOk) {
        FURI_LOG_W(TAG, "ucsi CONTROL write rejected: %d", (int)status);
    }
}

static int32_t usb_pd_worker(void* context) {
    UsbPd* instance = context;

    instance->event_loop = furi_event_loop_alloc();

    instance->ppm = ucsi_ppm_alloc();
    furi_check(instance->ppm);

    UcsiPpmConfig ppm_config;
    usb_pd_fill_ppm_config(instance, &ppm_config);

    const UcsiPpmStatus status = ucsi_ppm_init(instance->ppm, &ppm_config);
    if(status != UcsiPpmStatusOk) {
        FURI_LOG_E(TAG, "PPM init failed: %d", (int)status);
        ucsi_ppm_free(instance->ppm);
        instance->ppm = NULL;
        furi_event_loop_free(instance->event_loop);
        instance->event_loop = NULL;
        instance->init_ok = false;
        furi_event_flag_set(instance->init_flag, USB_PD_INIT_FLAG_DONE);
        return -1;
    }

    furi_event_loop_set_custom_event_callback(instance->event_loop, usb_pd_worker_custom_event_callback, instance);

    instance->sm_timer = furi_event_loop_timer_alloc(instance->event_loop, usb_pd_worker_sm_timer_callback, FuriEventLoopTimerTypeOnce, instance);

    if(instance->config.phy_poll_period_ms) {
        instance->phy_poll_timer = furi_event_loop_timer_alloc(instance->event_loop, usb_pd_worker_phy_poll_callback, FuriEventLoopTimerTypePeriodic, instance);
        furi_event_loop_timer_start(instance->phy_poll_timer, instance->config.phy_poll_period_ms);
    }

    /* Populate the register file image and snapshots and arm the first deadline before
     * anyone can observe us. */
    usb_pd_worker_epilogue(instance);

    if(instance->config.use_phy_irq) {
        furi_bsp_expander_main_attach_fusb302_callback(usb_pd_fusb302_callback, instance);
    }

    FURI_LOG_I(TAG, "up: FUSB302 @ 0x%02X", instance->config.fusb302_i2c_addr);

    instance->init_ok = true;
    furi_event_flag_set(instance->init_flag, USB_PD_INIT_FLAG_DONE);

    furi_event_loop_run(instance->event_loop);

    /* Teardown. Detach first: it blocks until an in-flight callback has
     * returned, so nothing references the event loop after this point (a
     * last-moment custom event on the stopped loop is harmless). */
    if(instance->config.use_phy_irq) {
        furi_bsp_expander_main_detach_fusb302_callback();
    }

    furi_event_loop_timer_free(instance->sm_timer);
    instance->sm_timer = NULL;
    if(instance->phy_poll_timer) {
        furi_event_loop_timer_free(instance->phy_poll_timer);
        instance->phy_poll_timer = NULL;
    }

    ucsi_ppm_deinit(instance->ppm);
    ucsi_ppm_free(instance->ppm);
    instance->ppm = NULL;

    furi_event_loop_free(instance->event_loop);
    instance->event_loop = NULL;

    return 0;
}

/* --- Public API ------------------------------------------------------------ */

void usb_pd_config_init_default(UsbPdConfig* config) {
    furi_check(config);
    memset(config, 0, sizeof(*config));

    config->fusb302_i2c_addr = USB_PD_DEFAULT_FUSB302_ADDR;
    config->use_phy_irq = true;
    config->phy_poll_period_ms = USB_PD_DEFAULT_PHY_POLL_MS;
    config->thread_stack_size = USB_PD_DEFAULT_STACK_SIZE;

    config->cc_operation_mode = UcsiPpmCcModeDrp;
    config->source_rp_current = UcsiPpmRpCurrent1A5;

    config->source_caps.pdos[0] = ucsi_ppm_pdo_fixed_source(5000, 3000, true, false, true, true);
    config->source_caps.count = 1;
    config->sink_caps.pdos[0] = ucsi_ppm_pdo_fixed_sink(5000, 3000, true, false, false, true, true);
    config->sink_caps.count = 1;

    config->supports_usb_pd = true;
    config->power_source_vbus = true;
    config->connector_usb2_capable = true;
}

UsbPd* usb_pd_alloc(const UsbPdConfig* config) {
    furi_check(config);
    furi_check(config->vbus_source_set);
    furi_check(config->power_supply_set);
    furi_check(config->use_phy_irq || config->phy_poll_period_ms);

    UsbPd* instance = malloc(sizeof(UsbPd));

    instance->config = *config;
    if(!instance->config.i2c_bus) instance->config.i2c_bus = &furi_hal_i2c_handle_main;
    if(!instance->config.fusb302_i2c_addr) {
        instance->config.fusb302_i2c_addr = USB_PD_DEFAULT_FUSB302_ADDR;
    }
    if(!instance->config.thread_stack_size) {
        instance->config.thread_stack_size = USB_PD_DEFAULT_STACK_SIZE;
    }

    instance->pubsub = furi_pubsub_alloc();
    instance->init_flag = furi_event_flag_alloc();

    instance->thread = furi_thread_alloc_ex("UsbPd", instance->config.thread_stack_size, usb_pd_worker, instance);
    furi_thread_start(instance->thread);

    furi_event_flag_wait(instance->init_flag, USB_PD_INIT_FLAG_DONE, FuriFlagWaitAny, FuriWaitForever);

    if(!instance->init_ok) {
        furi_thread_join(instance->thread);
        furi_thread_free(instance->thread);
        furi_event_flag_free(instance->init_flag);
        furi_pubsub_free(instance->pubsub);
        free(instance);
        return NULL;
    }

    return instance;
}

void usb_pd_free(UsbPd* instance) {
    furi_check(instance);

    furi_event_loop_set_custom_event(instance->event_loop, UsbPdLoopEventStop);
    furi_thread_join(instance->thread);
    furi_thread_free(instance->thread);

    furi_event_flag_free(instance->init_flag);
    furi_pubsub_free(instance->pubsub);
    free(instance);
}

FuriPubSub* usb_pd_get_pubsub(UsbPd* instance) {
    furi_check(instance);
    return instance->pubsub;
}

UcsiPpmConnectorState usb_pd_get_connector_state(UsbPd* instance) {
    furi_check(instance);
    return instance->state_snapshot;
}

bool usb_pd_get_contract(UsbPd* instance, UcsiPpmContractInfo* out) {
    furi_check(instance);
    furi_check(out);
    FURI_CRITICAL_ENTER();
    *out = instance->contract_snapshot;
    FURI_CRITICAL_EXIT();
    return out->contract_in_place;
}

bool usb_pd_ucsi_read(UsbPd* instance, uint16_t offset, uint16_t length, uint8_t* data) {
    furi_check(instance);
    furi_check(data);
    if((uint32_t)offset + (uint32_t)length > USB_PD_UCSI_REGFILE_SIZE) return false;
    if(length == 0) return true;

    FURI_CRITICAL_ENTER();
    memcpy(data, &instance->ucsi_regfile[offset], length);
    FURI_CRITICAL_EXIT();
    return true;
}

/* UCSI makes only CONTROL and MESSAGE_OUT writable by the OPM; VERSION, CCI
 * and MESSAGE_IN belong to the PPM. A write must fall entirely inside one of
 * the two writable fields — they are not adjacent, so nothing legal spans
 * them. */
static bool usb_pd_ucsi_is_opm_writable(uint32_t offset, uint32_t length) {
    const uint32_t end = offset + length;
    if(offset >= USB_PD_UCSI_OFFSET_CONTROL && end <= USB_PD_UCSI_OFFSET_CONTROL + USB_PD_UCSI_SIZE_CONTROL) {
        return true;
    }
    if(offset >= USB_PD_UCSI_OFFSET_MESSAGE_OUT && end <= USB_PD_UCSI_OFFSET_MESSAGE_OUT + USB_PD_UCSI_SIZE_MESSAGE_OUT) {
        return true;
    }
    return false;
}

bool usb_pd_ucsi_write(UsbPd* instance, uint16_t offset, uint16_t length, const uint8_t* data) {
    furi_check(instance);
    furi_check(data);
    if(length == 0) return true;
    if(!usb_pd_ucsi_is_opm_writable(offset, length)) return false;

    FURI_CRITICAL_ENTER();
    memcpy(&instance->ucsi_regfile[offset], data, length);
    FURI_CRITICAL_EXIT();

    /* UCSI doorbell: dispatch is gated on the LAST byte of CONTROL, so
     * byte-at-a-time hosts don't trigger a half-written command when the
     * opcode byte (CONTROL[0]) lands first. */
    const uint32_t control_end = USB_PD_UCSI_OFFSET_CONTROL + USB_PD_UCSI_SIZE_CONTROL;
    if(offset < control_end && (uint32_t)offset + length >= control_end) {
        furi_event_loop_set_custom_event(instance->event_loop, UsbPdLoopEventUcsiControl);
    }
    return true;
}

void usb_pd_reset(UsbPd* instance) {
    furi_check(instance);
    furi_event_loop_set_custom_event(instance->event_loop, UsbPdLoopEventReset);
}

void usb_pd_notify_power_supply_ready(UsbPd* instance) {
    furi_check(instance);
    furi_event_loop_set_custom_event(instance->event_loop, UsbPdLoopEventPowerReady);
}
