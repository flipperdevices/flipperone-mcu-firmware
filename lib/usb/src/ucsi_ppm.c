#include "ucsi_ppm.h"
#include "ucsi_ppm_i.h"
#include "ucsi_ppm_phy.h"
#include "ucsi_ppm_tc.h"

#include <stdlib.h>
#include <string.h>

// Two byte ranges [a_begin, a_end) and [b_begin, b_end) overlap
// iff a_begin < b_end && b_begin < a_end.
static bool ranges_overlap(uint16_t a_begin, uint16_t a_end, uint16_t b_begin, uint16_t b_end) {
    return a_begin < b_end && b_begin < a_end;
}

// Check whether a write range touches an OPM-read-only region (api.md §6):
// VERSION, CCI, MESSAGE_IN. RESERVED is not read-only — writes to it are
// silently dropped, not rejected.
static bool range_touches_readonly(uint16_t offset, uint16_t length) {
    const uint16_t end = (uint16_t)(offset + length);

    if(ranges_overlap(offset, end, UCSI_PPM_OFFSET_VERSION, UCSI_PPM_OFFSET_VERSION + UCSI_PPM_SIZE_VERSION)) {
        return true;
    }
    if(ranges_overlap(offset, end, UCSI_PPM_OFFSET_CCI, UCSI_PPM_OFFSET_CCI + UCSI_PPM_SIZE_CCI)) {
        return true;
    }
    if(ranges_overlap(offset, end, UCSI_PPM_OFFSET_MESSAGE_IN, UCSI_PPM_OFFSET_MESSAGE_IN + UCSI_PPM_SIZE_MESSAGE_IN)) {
        return true;
    }
    return false;
}

// Fixed Supply PDO at vSafe5V: bits 31:30 == 00b, voltage field == 100
// (5000 mV / 50 mV unit). See pd-scope.md §2.1, Table 6-9 / Table 6-14.
#define PDO_TYPE_MASK             0xC0000000u
#define PDO_TYPE_FIXED            0x00000000u
#define PDO_FIXED_VOLTAGE_FIELD   0x000FFC00u
#define PDO_FIXED_VOLTAGE_5V_BITS (100u << 10)

static bool pdo_is_fixed_5v(UcsiPpmPdo pdo) {
    if((pdo & PDO_TYPE_MASK) != PDO_TYPE_FIXED) return false;
    if((pdo & PDO_FIXED_VOLTAGE_FIELD) != PDO_FIXED_VOLTAGE_5V_BITS) return false;
    return true;
}

static bool config_is_valid(const UcsiPpmConfig* c) {
    // Required callbacks must be non-NULL (api.md §3 / §5.1).
    if(!c->time_ms) return false;
    if(!c->alert) return false;
    if(!c->i2c_read) return false;
    if(!c->i2c_write) return false;
    if(!c->gpio_write_vbus_source) return false;
    if(!c->power_supply_set) return false;
    if(!c->has_alt_power) return false;

    // FUSB302 I2C address (fusb302.md §addressing).
    if(c->fusb302_i2c_addr < 0x22 || c->fusb302_i2c_addr > 0x25) return false;

    // PDO lists: at least one entry, no more than max, first is Fixed 5V.
    if(c->source_caps.count == 0 || c->source_caps.count > UCSI_PPM_MAX_PDOS) return false;
    if(c->sink_caps.count == 0 || c->sink_caps.count > UCSI_PPM_MAX_PDOS) return false;
    if(!pdo_is_fixed_5v(c->source_caps.pdos[0])) return false;
    if(!pdo_is_fixed_5v(c->sink_caps.pdos[0])) return false;

    // Disabled CC mode requires the device to advertise Disabled State Support.
    if(c->initial_cc_operation_mode == UcsiPpmCcModeDisabled && !c->supports_disabled_state) {
        return false;
    }

    // bmPowerSource: at least one of AC / Other / VBUS must be set
    // (commands.md §2.6 GET_CAPABILITY bmAttributes).
    if(!c->power_source_ac && !c->power_source_other && !c->power_source_vbus) {
        return false;
    }

    return true;
}

// Fill regfile with initial values (api.md §5.1 step 3):
// VERSION = UCSI_PPM_VERSION_UCSI (BCD, little-endian 24 bits),
// everything else zero.
static void regfile_reset(UcsiPpm* ppm) {
    memset(ppm->regfile, 0, sizeof(ppm->regfile));
    ppm->regfile[UCSI_PPM_OFFSET_VERSION + 0] = (uint8_t)(UCSI_PPM_VERSION_UCSI & 0xFFu);
    ppm->regfile[UCSI_PPM_OFFSET_VERSION + 1] = (uint8_t)((UCSI_PPM_VERSION_UCSI >> 8) & 0xFFu);
    ppm->regfile[UCSI_PPM_OFFSET_VERSION + 2] = 0u;
}

UcsiPpm* ucsi_ppm_alloc(void) {
    UcsiPpm* ppm = (UcsiPpm*)malloc(sizeof(UcsiPpm));
    if(!ppm) return NULL;
    memset(ppm, 0, sizeof(*ppm));
    ppm->lifecycle = UcsiPpmLifecycleAllocated;
    return ppm;
}

void ucsi_ppm_free(UcsiPpm* ppm) {
    if(!ppm) return;
    if(ppm->lifecycle == UcsiPpmLifecycleInitialized) {
        // api.md §2.1: free auto-deinits if still initialized.
        (void)ucsi_ppm_deinit(ppm);
    }
    free(ppm);
}

UcsiPpmStatus ucsi_ppm_init(UcsiPpm* ppm, const UcsiPpmConfig* config) {
    if(!ppm || !config) return UcsiPpmStatusInvalidArg;
    if(ppm->lifecycle == UcsiPpmLifecycleInitialized) return UcsiPpmStatusAlreadyInitialized;
    if(!config_is_valid(config)) return UcsiPpmStatusInvalidConfig;

    ppm->config = *config;
    regfile_reset(ppm);
    ucsi_ppm_cmd_reset_state(ppm);
    ppm->pending_flags = 0u;

    // api.md §5.1 step 4: bring up L4 (FUSB302). Any I²C error stops init.
    UcsiPpmStatus s = ucsi_ppm_phy_init(ppm);
    if(s != UcsiPpmStatusOk) return UcsiPpmStatusHalError;

    // TODO: step 5 — kick L3 (Type-C SM) into initial Unattached.* state.

    ppm->lifecycle = UcsiPpmLifecycleInitialized;
    return UcsiPpmStatusOk;
}

UcsiPpmStatus ucsi_ppm_deinit(UcsiPpm* ppm) {
    if(!ppm) return UcsiPpmStatusInvalidArg;
    if(ppm->lifecycle != UcsiPpmLifecycleInitialized) return UcsiPpmStatusNotInitialized;

    // Tear down in reverse order: stop toggling first so SWITCHES0/1
    // resets in phy_deinit aren't fighting the toggle state machine.
    (void)ucsi_ppm_tc_deinit(ppm);
    (void)ucsi_ppm_phy_deinit(ppm);

    ppm->lifecycle = UcsiPpmLifecycleAllocated;
    return UcsiPpmStatusOk;
}

UcsiPpmStatus ucsi_ppm_reset(UcsiPpm* ppm) {
    if(!ppm) return UcsiPpmStatusInvalidArg;
    if(ppm->lifecycle != UcsiPpmLifecycleInitialized) return UcsiPpmStatusNotInitialized;

    regfile_reset(ppm);
    ucsi_ppm_cmd_reset_state(ppm);
    ppm->pending_flags = 0u;

    // api.md §5.2: full L4 re-init (SW_RESET + masks + INT_MASK clear) so
    // the chip is brought back to the same state as after init.
    UcsiPpmStatus s = ucsi_ppm_phy_init(ppm);
    if(s != UcsiPpmStatusOk) return UcsiPpmStatusHalError;

    s = ucsi_ppm_tc_reset(ppm);
    if(s != UcsiPpmStatusOk) return UcsiPpmStatusHalError;

    return UcsiPpmStatusOk;
}

UcsiPpmStatus ucsi_ppm_register_read(UcsiPpm* ppm, uint16_t offset, uint16_t length, uint8_t* buf) {
    if(!ppm || !buf) return UcsiPpmStatusInvalidArg;
    if(ppm->lifecycle != UcsiPpmLifecycleInitialized) return UcsiPpmStatusNotInitialized;
    if((uint32_t)offset + (uint32_t)length > UCSI_PPM_REGFILE_SIZE) {
        return UcsiPpmStatusInvalidArg;
    }
    if(length == 0) return UcsiPpmStatusOk;

    memcpy(buf, &ppm->regfile[offset], length);
    return UcsiPpmStatusOk;
}

UcsiPpmStatus ucsi_ppm_register_write(UcsiPpm* ppm, uint16_t offset, uint16_t length, const uint8_t* buf) {
    if(!ppm || !buf) return UcsiPpmStatusInvalidArg;
    if(ppm->lifecycle != UcsiPpmLifecycleInitialized) return UcsiPpmStatusNotInitialized;
    if((uint32_t)offset + (uint32_t)length > UCSI_PPM_REGFILE_SIZE) {
        return UcsiPpmStatusInvalidArg;
    }
    if(length == 0) return UcsiPpmStatusOk;
    if(range_touches_readonly(offset, length)) return UcsiPpmStatusInvalidArg;

    // Apply byte-by-byte to skip the RESERVED zones silently
    // (api.md §6: "Запись в RESERVED — игнор Ok").
    const uint16_t end = (uint16_t)(offset + length);
    for(uint16_t i = offset; i < end; ++i) {
        const bool in_reserved1 = (i == UCSI_PPM_OFFSET_RESERVED1);
        const bool in_reserved2 = (i == UCSI_PPM_OFFSET_RESERVED2);
        const bool in_reserved3 = (i == UCSI_PPM_OFFSET_RESERVED3);
        if(in_reserved1 || in_reserved2 || in_reserved3) continue;
        ppm->regfile[i] = buf[i - offset];
    }

    // A non-zero write to CONTROL[0] (Command opcode byte) triggers L2
    // command dispatch (architecture.md §2).
    if(offset == UCSI_PPM_OFFSET_CONTROL_COMMAND &&
       ppm->regfile[UCSI_PPM_OFFSET_CONTROL_COMMAND] != 0) {
        ucsi_ppm_cmd_dispatch(ppm);
    }

    return UcsiPpmStatusOk;
}

// L4 → L3 event sink. Today this only routes to the Type-C SM; once PRL/PE
// land they'll consume the PD-message-related events from the same stream.
static void phy_event_sink_to_tc(void* ctx, const UcsiPpmPhyEvent* event) {
    UcsiPpm* ppm = (UcsiPpm*)ctx;
    ucsi_ppm_tc_handle_phy_event(ppm, event);
}

UcsiPpmStatus ucsi_ppm_tick(UcsiPpm* ppm) {
    if(!ppm) return UcsiPpmStatusInvalidArg;
    if(ppm->lifecycle != UcsiPpmLifecycleInitialized) return UcsiPpmStatusNotInitialized;

    // Drain FUSB302 IRQ if the ISR-context caller flagged one.
    const uint32_t flags = ppm->pending_flags;
    if(flags & UCSI_PPM_PENDING_PHY_IRQ) {
        ppm->pending_flags = flags & ~UCSI_PPM_PENDING_PHY_IRQ;
        (void)ucsi_ppm_phy_pump(ppm, phy_event_sink_to_tc, ppm);
    }

    // TODO: power_supply_ready handling, L3 advancement, PD timeout checks,
    // CCI event delivery (api.md §5.3).

    return UcsiPpmStatusOk;
}

UcsiPpmStatus ucsi_ppm_notify_fusb302_irq(UcsiPpm* ppm) {
    if(!ppm) return UcsiPpmStatusInvalidArg;
    if(ppm->lifecycle != UcsiPpmLifecycleInitialized) return UcsiPpmStatusNotInitialized;
    // ISR-safe: single-writer-from-ISR, single-reader-from-tick (api.md §8).
    ppm->pending_flags |= UCSI_PPM_PENDING_PHY_IRQ;
    return UcsiPpmStatusOk;
}

UcsiPpmStatus ucsi_ppm_notify_power_supply_ready(UcsiPpm* ppm) {
    if(!ppm) return UcsiPpmStatusInvalidArg;
    if(ppm->lifecycle != UcsiPpmLifecycleInitialized) return UcsiPpmStatusNotInitialized;
    ppm->pending_flags |= UCSI_PPM_PENDING_POWER_SUPPLY_RDY;
    return UcsiPpmStatusOk;
}

UcsiPpmConnectorState ucsi_ppm_get_connector_state(const UcsiPpm* ppm) {
    if(!ppm) return UcsiPpmStateDisabled;
    if(ppm->lifecycle != UcsiPpmLifecycleInitialized) return UcsiPpmStateDisabled;
    switch((UcsiPpmTcState)ppm->tc_state) {
    case UcsiPpmTcStateDisabled:
        return UcsiPpmStateDisabled;
    case UcsiPpmTcStateUnattached:
        return UcsiPpmStateUnattached;
    case UcsiPpmTcStateAttachWait:
        return UcsiPpmStateAttachWait;
    case UcsiPpmTcStateAttachedSrc:
        return UcsiPpmStateAttachedSrc;
    case UcsiPpmTcStateAttachedSnk:
        return UcsiPpmStateAttachedSnk;
    case UcsiPpmTcStateErrorRecovery:
        return UcsiPpmStateErrorRecovery;
    }
    return UcsiPpmStateUnattached;
}

UcsiPpmStatus ucsi_ppm_get_contract(const UcsiPpm* ppm, UcsiPpmContractInfo* out) {
    if(!ppm || !out) return UcsiPpmStatusInvalidArg;
    if(ppm->lifecycle != UcsiPpmLifecycleInitialized) return UcsiPpmStatusNotInitialized;
    // TODO: populate from PE contract state.
    memset(out, 0, sizeof(*out));
    return UcsiPpmStatusOk;
}
