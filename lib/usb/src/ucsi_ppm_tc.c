#include "ucsi_ppm_tc.h"

#include "drivers/fusb302/fusb302_reg.h"

// Type-C CC debounce (Type-C R2.0 §5.3.3.5 — tCCDebounce, 100..200 ms).
// We use the nominal lower bound from plan/type-c-sm.md.
#define UCSI_PPM_TC_CC_DEBOUNCE_MS 100u

// Upper bound on time spent in AttachWait without seeing VBUS. The chip's
// I_VBUSOK should fire within tVbusON (~275 ms source side) or shortly
// after sink attaches; if nothing happens for 5 × CCDebounce we treat
// the would-be attach as bogus and re-arm toggling.
#define UCSI_PPM_TC_ATTACH_WAIT_TIMEOUT_MS 500u

// PD auto-retry count (PD R3.0 nRetryCount = 2 per plan/pd-scope.md §7).
#define UCSI_PPM_TC_PD_RETRIES 2u

// STATUS0.BC_LVL interpretation when measuring on the active CC pin with Rp
// enabled (source side, fusb302.md §5.1):
//   0b00 — < 200 mV   (no Rd, but rare; partner not present)
//   0b01 — Rd present, default-USB Rp current advertisement
//   0b10 — Rd present, 1.5 A capability
//   0b11 — > 1.23 V   (no Rd at all — partner detached, CC pulled by Rp)
#define UCSI_PPM_TC_BC_LVL_NO_RD 0b11u

// Maps the configured CC operation mode to the TOGGLE mode the chip
// should run in. Disabled returns false → caller skips phy_start_toggle.
static bool cc_mode_to_toggle(UcsiPpmCcOperationMode mode, UcsiPpmPhyToggleMode* out) {
    switch(mode) {
    case UcsiPpmCcModeRpOnly:
        *out = UcsiPpmPhyToggleModeSrc;
        return true;
    case UcsiPpmCcModeRdOnly:
        *out = UcsiPpmPhyToggleModeSnk;
        return true;
    case UcsiPpmCcModeDrp:
        *out = UcsiPpmPhyToggleModeDrp;
        return true;
    case UcsiPpmCcModeDisabled:
    default:
        return false;
    }
}

UcsiPpmStatus ucsi_ppm_tc_init(UcsiPpm* ppm) {
    ppm->tc_orientation = (int)UcsiPpmPhyCcNone;
    ppm->tc_role_is_src = false;

    UcsiPpmPhyToggleMode toggle_mode;
    if(!cc_mode_to_toggle(ppm->current_cc_operation_mode, &toggle_mode)) {
        // Disabled: terminations are off (phy_init leaves SWITCHES0 zeroed
        // post-SW_RESET); no toggle is needed.
        ppm->tc_state = (int)UcsiPpmTcStateDisabled;
        return UcsiPpmStatusOk;
    }

    ppm->tc_state = (int)UcsiPpmTcStateUnattached;
    return ucsi_ppm_phy_start_toggle(ppm, toggle_mode);
}

UcsiPpmStatus ucsi_ppm_tc_deinit(UcsiPpm* ppm) {
    // Stop the toggle so the chip doesn't keep cycling CC terminations
    // while we tear down. Best-effort.
    (void)ucsi_ppm_phy_stop_toggle(ppm);
    // Drop external VBUS source — if we were Attached.SRC we'd have raised
    // it. Idempotent on the caller side.
    ppm->config.gpio_write_vbus_source(ppm->config.hal_ctx, false);
    ppm->tc_state = (int)UcsiPpmTcStateDisabled;
    ppm->tc_orientation = (int)UcsiPpmPhyCcNone;
    ppm->tc_role_is_src = false;
    ppm->tc_vbus_seen = false;
    return UcsiPpmStatusOk;
}

UcsiPpmStatus ucsi_ppm_tc_reset(UcsiPpm* ppm) {
    return ucsi_ppm_tc_init(ppm);
}

// Decodes a TOGSS value into orientation + role. Returns false if the value
// is "still running" (0b000) or audio-accessory (0b111) — both unexpected
// here: 0 shouldn't appear with I_TOGDONE asserted, and we don't support
// audio mode (plan/fusb302.md §11).
static bool decode_togss(uint8_t togss, UcsiPpmPhyCc* out_cc, bool* out_is_src) {
    switch(togss) {
    case FUSB302_STATUS1A_TOGSS_SRCON_CC1:
        *out_cc = UcsiPpmPhyCc1;
        *out_is_src = true;
        return true;
    case FUSB302_STATUS1A_TOGSS_SRCON_CC2:
        *out_cc = UcsiPpmPhyCc2;
        *out_is_src = true;
        return true;
    case FUSB302_STATUS1A_TOGSS_SNKON_CC1:
        *out_cc = UcsiPpmPhyCc1;
        *out_is_src = false;
        return true;
    case FUSB302_STATUS1A_TOGSS_SNKON_CC2:
        *out_cc = UcsiPpmPhyCc2;
        *out_is_src = false;
        return true;
    default:
        return false;
    }
}

static void handle_toggle_done(UcsiPpm* ppm, const UcsiPpmPhyEvent* event) {
    if(ppm->tc_state != (int)UcsiPpmTcStateUnattached) {
        // Stray TOGDONE outside Unattached — drop. We don't restart toggle:
        // the connector may already be in a deliberate non-toggling state
        // (e.g. Disabled, ErrorRecovery in flight).
        return;
    }

    UcsiPpmPhyCc cc;
    bool is_src;
    if(!decode_togss(event->u.togss, &cc, &is_src)) {
        // Audio accessory or undefined — re-arm toggle in the same mode and
        // try again. (plan/type-c-sm.md treats audio as out of scope; we
        // bounce out of it rather than entering an audio state.)
        UcsiPpmPhyToggleMode toggle_mode;
        if(cc_mode_to_toggle(ppm->current_cc_operation_mode, &toggle_mode)) {
            (void)ucsi_ppm_phy_start_toggle(ppm, toggle_mode);
        }
        return;
    }

    ppm->tc_orientation = (int)cc;
    ppm->tc_role_is_src = is_src;
    ppm->tc_state = (int)UcsiPpmTcStateAttachWait;
    ppm->tc_attach_wait_start_ms = ppm->config.time_ms(ppm->config.hal_ctx);
    ppm->tc_vbus_seen = false;
    (void)ucsi_ppm_phy_lock_polarity(ppm, cc);
    // Source-side Rp current advert — once locked we drive the configured
    // Rp value so partner's Rd sees the right termination current. (No
    // effect on sink-side; HOST_CUR is gated by PU_EN*.)
    if(is_src) {
        (void)ucsi_ppm_phy_set_rp_current(ppm, ppm->config.source_rp_current);
        // Turn on the external VBUS source switch so the partner sees 5V.
        ppm->config.gpio_write_vbus_source(ppm->config.hal_ctx, true);
    }
    // Sink-side: nothing more — partner brings up VBUS and FUSB302's
    // I_VBUSOK will fire, taking us to Attached once the debounce expires.
}

// Commits AttachWait → Attached.{Src,Snk} when both conditions hold:
//   (a) VBUS has been observed (either we drove it as SRC and the chip's
//       I_VBUSOK fired, or partner provided it on SNK side).
//   (b) tCCDebounce has elapsed since AttachWait entry.
// Enables PD auto-CRC on entry to Attached so subsequent PD frames are
// accepted by the chip without per-frame software intervention.
static void tc_try_commit_attached(UcsiPpm* ppm) {
    if(ppm->tc_state != (int)UcsiPpmTcStateAttachWait) return;
    if(!ppm->tc_vbus_seen) return;
    const uint32_t now = ppm->config.time_ms(ppm->config.hal_ctx);
    const uint32_t elapsed = (uint32_t)(now - ppm->tc_attach_wait_start_ms);
    if(elapsed < UCSI_PPM_TC_CC_DEBOUNCE_MS) return;

    ppm->tc_state = ppm->tc_role_is_src ? (int)UcsiPpmTcStateAttachedSrc : (int)UcsiPpmTcStateAttachedSnk;
    (void)ucsi_ppm_phy_enable_pd(ppm, UCSI_PPM_TC_PD_RETRIES);
}

// Tears down the active session and returns to Unattached + re-armed toggle.
// Common detach / failed-attach path: drops AUTO_CRC, external VBUS source
// (source side only), CC routing, then restarts the chip's auto-toggle in
// the configured mode. If the connector is Disabled in config, falls back
// to Disabled instead.
static void tc_enter_unattached(UcsiPpm* ppm) {
    const bool was_src = ppm->tc_role_is_src;

    (void)ucsi_ppm_phy_disable_pd(ppm);
    if(was_src) {
        // Drop external VBUS source so we don't keep driving 5V into thin air.
        ppm->config.gpio_write_vbus_source(ppm->config.hal_ctx, false);
    }
    // Clear CC routing — TOGGLE will pick the new orientation when it settles.
    (void)ucsi_ppm_phy_lock_polarity(ppm, UcsiPpmPhyCcNone);

    ppm->tc_orientation = (int)UcsiPpmPhyCcNone;
    ppm->tc_role_is_src = false;
    ppm->tc_vbus_seen = false;

    UcsiPpmPhyToggleMode toggle_mode;
    if(cc_mode_to_toggle(ppm->current_cc_operation_mode, &toggle_mode)) {
        ppm->tc_state = (int)UcsiPpmTcStateUnattached;
        ppm->tc_attach_wait_start_ms = ppm->config.time_ms(ppm->config.hal_ctx);
        (void)ucsi_ppm_phy_start_toggle(ppm, toggle_mode);
    } else {
        // Disabled mode (or unreachable default): stay disabled.
        ppm->tc_state = (int)UcsiPpmTcStateDisabled;
    }
}

static void handle_vbus_changed(UcsiPpm* ppm, const UcsiPpmPhyEvent* event) {
    if(event->u.vbus_ok) {
        if(ppm->tc_state == (int)UcsiPpmTcStateAttachWait) {
            ppm->tc_vbus_seen = true;
            tc_try_commit_attached(ppm);
        }
        // vbus_ok=1 outside AttachWait is informational; PE/Type-C events
        // for already-Attached states use this for power-op-mode signalling
        // once those layers exist.
    } else {
        // VBUS lost while attached as Sink — partner went away.
        if(ppm->tc_state == (int)UcsiPpmTcStateAttachedSnk) {
            tc_enter_unattached(ppm);
        }
    }
}

static void handle_bc_lvl_changed(UcsiPpm* ppm, const UcsiPpmPhyEvent* event) {
    if(ppm->tc_state != (int)UcsiPpmTcStateAttachedSrc) return;
    // Source-side detach detection: Rd no longer pulling CC down → BC_LVL
    // rises to 0b11 (CC tracking our Rp pull-up alone).
    if(event->u.bc_lvl == UCSI_PPM_TC_BC_LVL_NO_RD) {
        tc_enter_unattached(ppm);
    }
}

void ucsi_ppm_tc_handle_phy_event(UcsiPpm* ppm, const UcsiPpmPhyEvent* event) {
    switch(event->kind) {
    case UcsiPpmPhyEventToggleDone:
        handle_toggle_done(ppm, event);
        break;
    case UcsiPpmPhyEventVbusChanged:
        handle_vbus_changed(ppm, event);
        break;
    case UcsiPpmPhyEventBcLvlChanged:
        handle_bc_lvl_changed(ppm, event);
        break;
    // TODO: CompChanged (MDAC-based source detach refinement), HardResetRx /
    // HardResetSent (PE coordination). PRL/PE events (MessageRx / TxSuccess /
    // TxRetryFail / Collision) are routed elsewhere when those layers exist.
    default:
        break;
    }
}

void ucsi_ppm_tc_tick(UcsiPpm* ppm) {
    if(ppm->tc_state == (int)UcsiPpmTcStateAttachWait) {
        // First try the normal commit path — debounce expired + VBUS seen.
        tc_try_commit_attached(ppm);
        // Still in AttachWait? Check for the give-up timeout: bogus attach
        // where partner left or never raised VBUS.
        if(ppm->tc_state == (int)UcsiPpmTcStateAttachWait) {
            const uint32_t now = ppm->config.time_ms(ppm->config.hal_ctx);
            const uint32_t elapsed = (uint32_t)(now - ppm->tc_attach_wait_start_ms);
            if(elapsed >= UCSI_PPM_TC_ATTACH_WAIT_TIMEOUT_MS) {
                tc_enter_unattached(ppm);
            }
        }
    }
}
