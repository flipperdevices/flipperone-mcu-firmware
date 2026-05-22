// Bring-up driver for lib/usb (UcsiPpm) against the real FUSB302.
//
// IMPORTANT: this app takes exclusive ownership of FUSB302 over the main I²C
// bus. Disable the existing `pd_srv` in applications/applications.c while
// running it — otherwise the two stacks race on register reads / writes and
// nothing works.
//
// Currently runs in DRP mode for exercising both roles: the chip toggles
// between Rp/Rd until a partner attaches, then settles to the matching role.
// VBUS source is driven by bq25792 OTG via the power service — vbus_source
// hook enables OTG at 5V/1.5A, psu_set retunes voltage/current on partner
// Request, and notify_power_supply_ready is fired synchronously so PE can
// emit PS_RDY. Note: in DRP without a partner the chip briefly settles as
// SRC every ~500 ms (false positive on Ra / noise) and we cycle OTG on/off
// — a Try.SRC delay in the TC layer would suppress this. The app loops on
// ucsi_ppm_tick (every 10 ms), prints state changes, and dumps the
// partner's Source PDOs once the PD contract is up.

#include <furi.h>
#include <furi_hal_i2c.h>
#include <furi_hal_i2c_config.h>
#include <power/power.h>

#include <ucsi_ppm.h>
#include <ucsi_ppm_config.h>

#include "ucsi_shim.h"

#define TAG "PdUcsi"

#define FUSB302_I2C_ADDR 0x22u
#define TICK_PERIOD_MS   10u
#define STATUS_LOG_MS    1000u

typedef struct {
    UcsiPpm* ppm;
    const FuriHalI2cBusHandle* i2c;
    Power* power;
    UcsiPpmConnectorState last_state;
    uint32_t last_log_tick;
    bool partner_caps_dumped;
} PdUcsi;

// --- HAL adapters ----------------------------------------------------------

static uint32_t hal_time_ms(void* ctx) {
    UNUSED(ctx);
    return furi_get_tick();
}

static UcsiPpmStatus hal_i2c_write(void* ctx, uint8_t addr, const uint8_t* data, size_t len) {
    PdUcsi* h = ctx;
    furi_hal_i2c_acquire(h->i2c);
    const int rc = furi_hal_i2c_master_tx_blocking(h->i2c, addr, data, len, FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(h->i2c);
    return rc >= 0 ? UcsiPpmStatusOk : UcsiPpmStatusHalError;
}

static UcsiPpmStatus hal_i2c_read(void* ctx, uint8_t addr, uint8_t* data, size_t len) {
    PdUcsi* h = ctx;
    furi_hal_i2c_acquire(h->i2c);
    // Standalone read fallback — only hit when i2c_write_read isn't wired.
    const int rc = furi_hal_i2c_master_rx_blocking(h->i2c, addr, data, len, FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(h->i2c);
    return rc >= 0 ? UcsiPpmStatusOk : UcsiPpmStatusHalError;
}

// Combined repeated-start txn — one atomic bus operation. Pairs the register
// pointer write with the data read so other drivers can't slip in between
// (FUSB302 would otherwise lose its address pointer and return garbage).
static UcsiPpmStatus hal_i2c_write_read(void* ctx, uint8_t addr, const uint8_t* tx, size_t tx_len, uint8_t* rx, size_t rx_len) {
    PdUcsi* h = ctx;
    furi_hal_i2c_acquire(h->i2c);
    const int rc = furi_hal_i2c_master_trx_blocking(h->i2c, addr, tx, tx_len, rx, rx_len, FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(h->i2c);
    return rc >= 0 ? UcsiPpmStatusOk : UcsiPpmStatusHalError;
}

static void hal_vbus_source_write(void* ctx, bool value) {
    PdUcsi* h = ctx;
    FURI_LOG_I(TAG, "vbus_source(%s)", value ? "on" : "off");
    if(value) {
        // Default to vSafe5V at 1.5 A — matches our advertised Source PDO.
        // PE will retune via psu_set once the partner Requests a different
        // voltage during contract negotiation.
        if(!power_bq25792_set_otg_params(h->power, 5000u, 1500u)) {
            FURI_LOG_W(TAG, "bq25792_set_otg_params(5V/1.5A) failed");
        }
        if(!power_bq25792_otg_enable(h->power, true)) {
            FURI_LOG_W(TAG, "bq25792_otg_enable(true) failed");
        }
    } else {
        (void)power_bq25792_otg_enable(h->power, false);
    }
}

static void hal_vbus_discharge_write(void* ctx, bool value) {
    UNUSED(ctx);
    UNUSED(value);
}

static UcsiPpmStatus hal_psu_set(void* ctx, uint16_t voltage_mv, uint16_t current_limit_ma) {
    PdUcsi* h = ctx;
    FURI_LOG_I(TAG, "psu_set(%u mV, %u mA)", voltage_mv, current_limit_ma);
    if(!power_bq25792_set_otg_params(h->power, voltage_mv, current_limit_ma)) {
        FURI_LOG_W(TAG, "bq25792_set_otg_params failed");
        return UcsiPpmStatusHalError;
    }
    // PE waits for notify_power_supply_ready before emitting PS_RDY. We
    // don't model the ramp delay — fine for bring-up, the chip-side ramp
    // is sub-tick. Add a debounce / readback if partners reject the contract
    // due to a fast settle.
    ucsi_ppm_notify_power_supply_ready(h->ppm);
    return UcsiPpmStatusOk;
}

static bool hal_has_alt_power(void* ctx) {
    UNUSED(ctx);
    return false;
}

static void hal_alert(void* ctx) {
    UNUSED(ctx);
    // The CCI alert just tells OPM "something to read" — for bring-up we
    // already poll, so this is informational.
}

static void hal_log(void* ctx, UcsiPpmLogLevel level, const char* module, const char* fmt, va_list args) {
    UNUSED(ctx);
    char buf[160];
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

// --- config & lifecycle ----------------------------------------------------

static void config_fill(UcsiPpmConfig* cfg, PdUcsi* host) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->hal_ctx = host;
    cfg->time_ms = hal_time_ms;
    cfg->alert = hal_alert;
    cfg->i2c_read = hal_i2c_read;
    cfg->i2c_write = hal_i2c_write;
    cfg->i2c_write_read = hal_i2c_write_read;
    cfg->gpio_write_vbus_source = hal_vbus_source_write;
    cfg->gpio_write_vbus_discharge = hal_vbus_discharge_write;
    cfg->power_supply_set = hal_psu_set;
    cfg->has_alt_power = hal_has_alt_power;
    cfg->log = hal_log;
    cfg->fusb302_i2c_addr = FUSB302_I2C_ADDR;

    // DRP — toggle between Rp/Rd until a partner attaches
    cfg->initial_cc_operation_mode = UcsiPpmCcModeDrp;
    cfg->source_rp_current = UcsiPpmRpCurrent1A5;

    // Host-port-like Source caps (5V/1500 mA) and a Sink ask for 5V/3 A.
    cfg->source_caps.pdos[0] = ucsi_ppm_pdo_fixed_source(5000, 1500, true /*DRP*/, false, true /*USBcom*/, true /*DRD*/);
    cfg->source_caps.count = 1;
    cfg->sink_caps.pdos[0] = ucsi_ppm_pdo_fixed_sink(5000, 3000, true, false, false, true, true);
    cfg->sink_caps.count = 1;

    cfg->supports_usb_pd = true;
    cfg->power_source_vbus = true;
    cfg->connector_usb2_capable = true;
}

static const char* state_str(UcsiPpmConnectorState s) {
    switch(s) {
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

// Direct FUSB302 register dump — bypasses lib, talks to the chip via the same
// HAL we plumbed for the lib. Useful at attach commit to verify the chip is
// actually in the configuration the lib thinks it is.
static void dump_fusb302_regs(PdUcsi* host) {
    static const struct {
        uint8_t reg;
        const char* name;
    } regs[] = {
        {0x02, "Sw0"},
        {0x03, "Sw1"},
        {0x06, "Ctl0"},
        {0x08, "Ctl2"},
        {0x09, "Ctl3"},
        {0x0A, "Mask"},
        {0x0E, "MaskA"},
        {0x0F, "MaskB"},
        {0x40, "Sts0"},
        {0x41, "Sts1"},
        {0x3D, "Sts1a"},
    };
    for(size_t i = 0; i < sizeof(regs) / sizeof(regs[0]); ++i) {
        uint8_t v = 0;
        if(hal_i2c_write_read(host, FUSB302_I2C_ADDR, &regs[i].reg, 1, &v, 1) == UcsiPpmStatusOk) {
            FURI_LOG_I(TAG, "fusb302 %s[0x%02X]=0x%02X", regs[i].name, regs[i].reg, v);
        } else {
            FURI_LOG_W(TAG, "fusb302 read 0x%02X failed", regs[i].reg);
        }
    }
}

// Dumps partner Source caps the first time we see a live PD contract during
// an attach session, then clears the flag on detach so the next attach gets
// a fresh dump.
static void maybe_dump_partner_caps(PdUcsi* host) {
    const bool attached = host->last_state == UcsiPpmStateAttachedSnk || host->last_state == UcsiPpmStateAttachedSrc;
    if(!attached) {
        host->partner_caps_dumped = false;
        return;
    }
    if(host->partner_caps_dumped) return;

    UcsiPpmContractInfo c = {0};
    if(ucsi_ppm_get_contract(host->ppm, &c) != UcsiPpmStatusOk || !c.contract_in_place) return;

    ucsi_shim_dump_partner_source_caps(host->ppm);
    host->partner_caps_dumped = true;
}

static int32_t pd_ucsi_thread(void* arg) {
    PdUcsi* host = arg;

    UcsiPpmConfig cfg;
    config_fill(&cfg, host);

    host->ppm = ucsi_ppm_alloc();
    furi_check(host->ppm);
    const UcsiPpmStatus s = ucsi_ppm_init(host->ppm, &cfg);
    if(s != UcsiPpmStatusOk) {
        FURI_LOG_E(TAG, "ucsi_ppm_init failed: %d", (int)s);
        ucsi_ppm_free(host->ppm);
        return -1;
    }
    FURI_LOG_I(TAG, "PPM up — sink-only, FUSB302 @ 0x%02X", FUSB302_I2C_ADDR);

    // Enable every notification we care about (CSC bits 1..15 + Command
    // Completed at bit 0).
    if(!ucsi_shim_set_notification_enable(host->ppm, 0x1FFFFu)) {
        FURI_LOG_W(TAG, "SET_NOTIFICATION_ENABLE failed");
    }

    uint8_t cap_buf[16];
    if(ucsi_shim_get_capability(host->ppm, cap_buf)) {
        FURI_LOG_I(TAG, "cap: bcdPD=%02X%02X bcdTC=%02X%02X", cap_buf[13], cap_buf[12], cap_buf[15], cap_buf[14]);
    }

    host->last_state = ucsi_ppm_get_connector_state(host->ppm);
    host->last_log_tick = furi_get_tick();
    host->partner_caps_dumped = false;

    while(true) {
        // No INT GPIO wired yet — poll FUSB302 every tick. phy_pump reads
        // INTERRUPT/A/B and only emits events on real changes, so a quiet
        // chip stays quiet. Swap for furi_bsp_expander_main_attach_fusb302_callback
        // once we want event-driven operation.
        ucsi_ppm_notify_fusb302_irq(host->ppm);
        ucsi_ppm_tick(host->ppm);

        const UcsiPpmConnectorState now = ucsi_ppm_get_connector_state(host->ppm);
        if(now != host->last_state) {
            FURI_LOG_I(TAG, "state: %s → %s", state_str(host->last_state), state_str(now));
            host->last_state = now;
            ucsi_shim_log_status(host->ppm);
            // if(now == UcsiPpmStateAttachedSnk || now == UcsiPpmStateAttachedSrc) {
            //     dump_fusb302_regs(host);
            // }
            UNUSED(dump_fusb302_regs);
        }

        if(furi_get_tick() - host->last_log_tick >= STATUS_LOG_MS) {
            host->last_log_tick = furi_get_tick();
            // Quiet poll: only print when PE/TC raised a CSC bit since last
            // read. Initial / forced log already happened on state change above.
            ucsi_shim_log_status_if_changed(host->ppm);
        }

        maybe_dump_partner_caps(host);

        furi_delay_ms(TICK_PERIOD_MS);
    }
}

// --- app entrypoint --------------------------------------------------------

int32_t pd_ucsi_app(void* p) {
    UNUSED(p);
    PdUcsi host = {0};
    host.i2c = &furi_hal_i2c_handle_main;
    host.power = furi_record_open(RECORD_POWER);
    const int32_t rc = pd_ucsi_thread(&host);
    furi_record_close(RECORD_POWER);
    return rc;
}
