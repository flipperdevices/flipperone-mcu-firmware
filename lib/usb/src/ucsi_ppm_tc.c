#include "ucsi_ppm_tc.h"

#include "ucsi_ppm_pe.h"
#include "ucsi_ppm_prl.h"

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
// vRa range — partner has Ra termination only (Type-A→C adapter, cable plug),
// not a real Rd-presenting sink. We initially settle Attached.SRC on a brief
// CC glitch and then see BC_LVL drop here, which is our cue to bail.
#define UCSI_PPM_TC_BC_LVL_RA    0b00u

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
    if(is_src) {
        // Program Rp termination current so partner's Rd produces the right
        // voltage divider, then explicitly assert PU_EN — the chip's toggle
        // FSM doesn't keep PU_EN latched after I_TOGDONE, and without it CC
        // floats and BC_LVL reads vNoRd. Do NOT enable external VBUS yet;
        // driving 5 V into VBUS during the debounce window can bleed into
        // CC sense. VBUS comes up at commit (see tc_try_commit_attached).
        (void)ucsi_ppm_phy_set_rp_current(ppm, ppm->config.source_rp_current);
        (void)ucsi_ppm_phy_set_source_termination(ppm, cc);
    }
    // Sink-side: nothing more — partner brings up VBUS and FUSB302's
    // I_VBUSOK will fire, taking us to Attached once the debounce expires.
}

// Forward decl — tc_try_commit_attached can bail out to Unattached on
// invalid CC state, but tc_enter_unattached lives further down.
static void tc_enter_unattached(UcsiPpm* ppm);

// Commits AttachWait → Attached.{Src,Snk} when both conditions hold:
//   (a) VBUS has been observed (either we drove it as SRC and the chip's
//       I_VBUSOK fired, or partner provided it on SNK side).
//   (b) tCCDebounce has elapsed since AttachWait entry.
// Enables PD auto-CRC on entry to Attached so subsequent PD frames are
// accepted by the chip without per-frame software intervention.
static void tc_try_commit_attached(UcsiPpm* ppm) {
    if(ppm->tc_state != (int)UcsiPpmTcStateAttachWait) return;
    const uint32_t now = ppm->config.time_ms(ppm->config.hal_ctx);
    const uint32_t elapsed = (uint32_t)(now - ppm->tc_attach_wait_start_ms);
    if(elapsed < UCSI_PPM_TC_CC_DEBOUNCE_MS) return;

    if(ppm->tc_role_is_src) {
        // Validate steady-state BC_LVL before committing — the chip can
        // settle SRC on CC glitches (Ra-only adapters, partner with Rd on
        // the opposite CC, noise). Require BC_LVL inside the vRd window
        // expected for our configured Rp current:
        //   • Default Rp (80 μA)  → Rd ≈ 0.4 V  → BC_LVL = 0b01
        //   • 1.5A Rp   (180 μA) → Rd ≈ 0.92 V → BC_LVL = 0b10
        //   • 3A Rp     (330 μA) → Rd ≈ 1.68 V → BC_LVL = 0b11 (colliding
        //     with vNoRd; can't distinguish via BC_LVL alone — accept it
        //     and rely on COMP-based detach instead).
        uint8_t bc_lvl = 0u;
        if(ucsi_ppm_phy_read_bc_lvl(ppm, &bc_lvl) != UcsiPpmStatusOk) {
            tc_enter_unattached(ppm);
            return;
        }
        bool bc_valid;
        switch(ppm->config.source_rp_current) {
        case UcsiPpmRpCurrentUsbDefault:
            bc_valid = (bc_lvl == 0b01u);
            break;
        case UcsiPpmRpCurrent1A5:
            bc_valid = (bc_lvl == 0b10u);
            break;
        case UcsiPpmRpCurrent3A:
            // No safe BC_LVL discriminator — accept anything not clearly
            // vRa. COMP-based detach catches the bad cases later.
            bc_valid = (bc_lvl != UCSI_PPM_TC_BC_LVL_RA);
            break;
        default:
            bc_valid = false;
            break;
        }
        if(!bc_valid) {
            tc_enter_unattached(ppm);
            return;
        }
        // We're the source so we generate VBUS ourselves — VBUSOK from
        // the chip's VBUS pin will fire shortly afterwards but we don't
        // gate the commit on it. Enable the external switch now so the
        // partner sees 5 V at the start of Attached.SRC.
        ppm->config.gpio_write_vbus_source(ppm->config.hal_ctx, true);
    } else {
        // Sink-side: partner must have provided VBUS by now. The event path
        // sets tc_vbus_seen on the I_VBUSOK edge, but if the partner was
        // already attached before phy_init() ran the chip starts up with
        // VBUSOK latched high and no edge ever fires. Fall back to a direct
        // STATUS0.VBUSOK poll here so we still commit.
        if(!ppm->tc_vbus_seen) {
            bool vbus_ok = false;
            if(ucsi_ppm_phy_read_vbusok(ppm, &vbus_ok) != UcsiPpmStatusOk || !vbus_ok) {
                return;
            }
            ppm->tc_vbus_seen = true;
        }
    }

    ppm->tc_state = ppm->tc_role_is_src ? (int)UcsiPpmTcStateAttachedSrc : (int)UcsiPpmTcStateAttachedSnk;
    (void)ucsi_ppm_phy_enable_pd(ppm, UCSI_PPM_TC_PD_RETRIES);

    // Hand off to PE: source-side starts retransmitting Source_Capabilities,
    // sink-side arms SinkWaitCapTimer.
    if(ppm->tc_role_is_src) {
        ucsi_ppm_pe_on_attach_src(ppm);
    } else {
        ucsi_ppm_pe_on_attach_snk(ppm);
    }

    // Tell OPM the connector just lit up. Power Operation Mode change is
    // also raised because Connect Status flipped to 1 (the partner type is
    // now meaningful — see GET_CONNECTOR_STATUS).
    ucsi_ppm_notify_connector_change(ppm, UCSI_PPM_CSC_CONNECT_CHANGE | UCSI_PPM_CSC_PARTNER_CHANGED | UCSI_PPM_CSC_POWER_OP_MODE_CHANGE);
}

// Tears down the active session and returns to Unattached + re-armed toggle.
// Common detach / failed-attach path: drops AUTO_CRC, external VBUS source
// (source side only), CC routing, then restarts the chip's auto-toggle in
// the configured mode. If the connector is Disabled in config, falls back
// to Disabled instead.
static void tc_enter_unattached(UcsiPpm* ppm) {
    const bool was_src = ppm->tc_role_is_src;
    const bool was_attached = ppm->tc_state == (int)UcsiPpmTcStateAttachedSrc || ppm->tc_state == (int)UcsiPpmTcStateAttachedSnk;

    if(was_src) {
        // Drop external VBUS source so we don't keep driving 5V into thin air.
        ppm->config.gpio_write_vbus_source(ppm->config.hal_ctx, false);
    }
    // Full chip re-init on detach. Cheaper to redo than to chase leftover
    // role bits (PU_EN from a source attach poisoning the next sink try,
    // sticky RXSOP1 in STATUS1, stale MDAC, masked-out interrupts, etc.).
    // phy_init covers SW_RESET → Power → masks → INT_MASK clear in one shot;
    // the subsequent start_toggle re-arms the FSM cleanly.
    (void)ucsi_ppm_phy_init(ppm);

    ppm->tc_orientation = (int)UcsiPpmPhyCcNone;
    ppm->tc_role_is_src = false;
    ppm->tc_vbus_seen = false;

    // PD session ended — clear MessageID counters so a fresh attach starts
    // from MsgID=0 on both directions (PD R3.0 §6.8.1).
    (void)ucsi_ppm_prl_reset(ppm);
    ucsi_ppm_pe_on_detach(ppm);

    // Tell OPM the partner is gone — but only on a genuine detach from
    // Attached.*. A failed-attach drop (AttachWait → Unattached without
    // ever committing) doesn't change OPM-visible Connect Status.
    if(was_attached) {
        ucsi_ppm_notify_connector_change(ppm, UCSI_PPM_CSC_CONNECT_CHANGE | UCSI_PPM_CSC_PARTNER_CHANGED | UCSI_PPM_CSC_POWER_OP_MODE_CHANGE);
    }

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
        // VBUS lost while attached as Sink — partner went away, unless
        // we're in the middle of a PR_Swap where the partner's VBUS drop is
        // an expected protocol step (PD R3.0 §6.6.x).
        if(ppm->tc_state == (int)UcsiPpmTcStateAttachedSnk &&
           !ucsi_ppm_pe_pr_swap_in_progress(ppm)) {
            tc_enter_unattached(ppm);
        }
    }
}

static void handle_bc_lvl_changed(UcsiPpm* ppm, const UcsiPpmPhyEvent* event) {
    if(ppm->tc_state != (int)UcsiPpmTcStateAttachedSrc) return;
    // Source-side detach detection. Two valid "partner gone" signatures:
    //   • BC_LVL = 0b11 — Rd removed, CC pulls up to the Rp source rail
    //   • BC_LVL = 0b00 — CC sits in vRa range (Type-A→C adapter / cable
    //     plug only — no real Rd-presenting sink). Common with USB-A
    //     accessories plugged via passive adapters; the chip momentarily
    //     settles SRC during DRP toggle on a CC glitch and then steady-
    //     state BC_LVL exposes the missing Rd.
    if(event->u.bc_lvl == UCSI_PPM_TC_BC_LVL_NO_RD ||
       event->u.bc_lvl == UCSI_PPM_TC_BC_LVL_RA) {
        tc_enter_unattached(ppm);
    }
}

static void handle_comp_changed(UcsiPpm* ppm, const UcsiPpmPhyEvent* event) {
    // Source-side detach via the MDAC comparator. When partner Rd is present,
    // CC sits well below MDAC and COMP=0; once Rd is removed, CC pulls up to
    // ~Rp pull-up rail and COMP latches above MDAC. BC_LVL also rises but is
    // sometimes noisy in source mode — COMP is the more reliable trigger per
    // FUSB302 datasheet §4.5.
    if(ppm->tc_state != (int)UcsiPpmTcStateAttachedSrc) return;
    if(event->u.comp_above) {
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
    case UcsiPpmPhyEventCompChanged:
        handle_comp_changed(ppm, event);
        break;
    // PRL/PE events (MessageRx / TxSuccess / TxRetryFail / Collision /
    // HardReset*) are dispatched elsewhere.
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
