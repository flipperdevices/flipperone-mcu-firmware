#include "ucsi_ppm_tc.h"

#include "drivers/fusb302/fusb302_reg.h"

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
    ppm->tc_state = (int)UcsiPpmTcStateDisabled;
    ppm->tc_orientation = (int)UcsiPpmPhyCcNone;
    ppm->tc_role_is_src = false;
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
    (void)ucsi_ppm_phy_lock_polarity(ppm, cc);
    // Source-side Rp current advert — once locked we drive the configured
    // Rp value so partner's Rd sees the right termination current. (No
    // effect on sink-side; HOST_CUR is gated by PU_EN*.)
    if(is_src) {
        (void)ucsi_ppm_phy_set_rp_current(ppm, ppm->config.source_rp_current);
    }
    // TODO (1b): start CCDebounce/PDDebounce timers and transition to
    // AttachedSrc/AttachedSnk on expiry; enable PD on AttachedSrc.
}

void ucsi_ppm_tc_handle_phy_event(UcsiPpm* ppm, const UcsiPpmPhyEvent* event) {
    switch(event->kind) {
    case UcsiPpmPhyEventToggleDone:
        handle_toggle_done(ppm, event);
        break;
    // TODO (1b/1c): VbusChanged, CompChanged, BcLvlChanged, HardResetRx,
    // HardResetSent. PRL/PE events (MessageRx/TxSuccess/TxRetryFail/Collision)
    // are routed elsewhere when those layers exist.
    default:
        break;
    }
}
