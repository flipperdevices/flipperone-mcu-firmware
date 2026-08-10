#include "ucsi_ppm_tc.h"

#include "ucsi_ppm_pe.h"
#include "ucsi_ppm_prl.h"

#include "drivers/fusb302/fusb302_reg.h"

#define TAG "UcsiTc"

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

// tPDDebounce (Type-C R2.0 Table 4-30, 10..20 ms): how long a CC level has to
// hold before we believe it. Readings taken while the line carries BMC data
// are discarded outright, but even idle ones need to settle.
#define UCSI_PPM_TC_PD_DEBOUNCE_MS 15u

// tSrcRecover upper bound (PD R3.0 Table 7-24, 0.66..1.0 s): how long a
// source may keep VBUS down during a Hard Reset. Losing VBUS as a sink only
// counts as a detach once this expires without it coming back.
#define UCSI_PPM_TC_SRC_RECOVER_MS 1000u

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

// Sink side the same encoding means the opposite thing: CC sits below 200 mV
// because no source Rp is pulling it up through our Rd. That is the real
// Type-C detach condition for a sink — VBUS alone is not (see
// handle_vbus_changed).
#define UCSI_PPM_TC_BC_LVL_SNK_NO_RP 0b00u

// Sink side, BC_LVL measured on the active CC pin tells us which Rp the
// source is advertising, i.e. how much we may draw before any PD contract
// (Type-C R2.0 Table 4-25). "Default USB" is 500 mA on USB 2.0 and 900 mA on
// USB 3.x; we cannot tell them apart here, so take the safe one.
static uint16_t tc_rp_advertised_current_ma(uint8_t bc_lvl) {
    switch(bc_lvl) {
    case 0b01u:
        return 500u;
    case 0b10u:
        return 1500u;
    case 0b11u:
        return 3000u;
    default:
        return 0u; // 0b00 — no Rp on CC at all
    }
}

void ucsi_ppm_tc_update_sink_current_limit(UcsiPpm* ppm) {
    if(!ppm->config.sink_current_limit) return;

    uint16_t current_ma = 0u;
    UcsiPpmSinkLimitSource source = UcsiPpmSinkLimitNone;

    if(ppm->tc_state == (int)UcsiPpmTcStateAttachedSnk) {
        if(ppm->pe_state == (int)UcsiPpmPeSnkReady) {
            // An explicit contract outranks the Rp advertisement.
            current_ma = ppm->pe_negotiated_current_ma;
            source = UcsiPpmSinkLimitPdContract;
        } else {
            current_ma = tc_rp_advertised_current_ma(ppm->tc_last_bc_lvl);
            // Only once PE has given up on PD is the advertisement final.
            // Until then negotiation may still be in flight, and treating
            // the number as a licence to go looking for more wrecks it.
            source = ppm->pe_typec_only ? UcsiPpmSinkLimitTypeCOnly : UcsiPpmSinkLimitTypeC;
        }
    }

    const bool unchanged = ppm->sink_current_limit_valid &&
                           ppm->sink_current_limit_ma == current_ma &&
                           ppm->sink_current_limit_source == source;
    if(unchanged) return;

    ppm->sink_current_limit_ma = current_ma;
    ppm->sink_current_limit_source = source;
    ppm->sink_current_limit_valid = true;

    static const char* const source_names[] = {"none", "type-c Rp", "type-c only", "pd contract"};
    UCSI_LOG_I(ppm, "sink limit: %u mA (%s)", (unsigned)current_ma, source_names[source]);
    ppm->config.sink_current_limit(ppm->config.hal_ctx, current_ma, source);
}

static const char* tc_state_str(int state) {
    switch((UcsiPpmTcState)state) {
    case UcsiPpmTcStateDisabled:
        return "Disabled";
    case UcsiPpmTcStateUnattached:
        return "Unattached";
    case UcsiPpmTcStateAttachWait:
        return "AttachWait";
    case UcsiPpmTcStateAttachedSrc:
        return "Attached.SRC";
    case UcsiPpmTcStateAttachedSnk:
        return "Attached.SNK";
    case UcsiPpmTcStateErrorRecovery:
        return "ErrorRecovery";
    }
    return "?";
}

// Single funnel for every Type-C state change so no transition can happen
// without saying why. Silent when the state does not actually change.
static void tc_set_state(UcsiPpm* ppm, int new_state, const char* reason) {
    if(ppm->tc_state == new_state) return;
    UCSI_LOG_I(ppm, "%s -> %s (%s)", tc_state_str(ppm->tc_state), tc_state_str(new_state), reason);
    ppm->tc_state = new_state;
}

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
        tc_set_state(ppm, (int)UcsiPpmTcStateDisabled, "init, cc mode disabled");
        return UcsiPpmStatusOk;
    }

    tc_set_state(ppm, (int)UcsiPpmTcStateUnattached, "init");
    return ucsi_ppm_phy_start_toggle(ppm, toggle_mode);
}

UcsiPpmStatus ucsi_ppm_tc_deinit(UcsiPpm* ppm) {
    // Stop the toggle so the chip doesn't keep cycling CC terminations
    // while we tear down. Best-effort.
    (void)ucsi_ppm_phy_stop_toggle(ppm);
    // Drop external VBUS source — if we were Attached.SRC we'd have raised
    // it. Idempotent on the caller side.
    ppm->config.gpio_write_vbus_source(ppm->config.hal_ctx, false);
    tc_set_state(ppm, (int)UcsiPpmTcStateDisabled, "deinit");
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
    UCSI_LOG_I(ppm, "togss settled: cc%d, role %s", (int)cc, is_src ? "SRC" : "SNK");
    tc_set_state(ppm, (int)UcsiPpmTcStateAttachWait, "toggle settled");
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
static void tc_enter_unattached(UcsiPpm* ppm, const char* reason);

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
            tc_enter_unattached(ppm, "src commit: bc_lvl read failed");
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
            tc_enter_unattached(ppm, "src commit: bc_lvl not a valid Rd");
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
        // Seed the Rp advertisement. The chip only raises I_BC_LVL on a
        // change, so without this first read we would not know what the
        // source allows until it happens to change its mind. Taken as-is
        // rather than through the debouncer — there is no previous value to
        // debounce against, and a later I_BC_LVL corrects it if the line
        // happened to be busy just now.
        uint8_t bc_lvl = 0u;
        if(ucsi_ppm_phy_read_bc_lvl(ppm, &bc_lvl) == UcsiPpmStatusOk) {
            ppm->tc_last_bc_lvl = bc_lvl;
        }
        ppm->tc_bc_lvl_pending_valid = false;
        ppm->tc_vbus_lost = false;
    }

    tc_set_state(
        ppm,
        ppm->tc_role_is_src ? (int)UcsiPpmTcStateAttachedSrc : (int)UcsiPpmTcStateAttachedSnk,
        "debounce expired, vbus ok");
    (void)ucsi_ppm_phy_enable_pd(ppm, UCSI_PPM_TC_PD_RETRIES);
    ucsi_ppm_phy_log_config(ppm, "pd enabled");

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
static void tc_enter_unattached(UcsiPpm* ppm, const char* reason) {
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
    // Per-session CC state must not leak into the next attach.
    ppm->tc_last_bc_lvl = 0u;
    ppm->tc_bc_lvl_pending_valid = false;
    ppm->tc_vbus_lost = false;

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
        tc_set_state(ppm, (int)UcsiPpmTcStateUnattached, reason);
        ppm->tc_attach_wait_start_ms = ppm->config.time_ms(ppm->config.hal_ctx);
        (void)ucsi_ppm_phy_start_toggle(ppm, toggle_mode);
    } else {
        // Disabled mode (or unreachable default): stay disabled.
        tc_set_state(ppm, (int)UcsiPpmTcStateDisabled, reason);
    }
}

static void handle_vbus_changed(UcsiPpm* ppm, const UcsiPpmPhyEvent* event) {
    if(event->u.vbus_ok) {
        if(ppm->tc_state == (int)UcsiPpmTcStateAttachWait) {
            ppm->tc_vbus_seen = true;
            tc_try_commit_attached(ppm);
        }
        // Back on its feet — whatever pulled it down did not amount to a
        // detach after all.
        if(ppm->tc_vbus_lost) {
            ppm->tc_vbus_lost = false;
            UCSI_LOG_I(ppm, "vbus recovered");
        }
    } else {
        // VBUS lost while attached as Sink. Not a detach on its own: a Hard
        // Reset makes the source drop VBUS to vSafe0V and bring it back
        // within tSrcRecover (PD R3.0 §6.6.10), and a PR_Swap has the partner
        // drop it deliberately too (§6.6.x). Start the clock instead — tc_tick
        // declares the detach only if VBUS fails to return.
        if(ppm->tc_state != (int)UcsiPpmTcStateAttachedSnk) return;
        if(ucsi_ppm_pe_pr_swap_in_progress(ppm)) return;
        if(ppm->tc_vbus_lost) return;

        ppm->tc_vbus_lost = true;
        ppm->tc_vbus_lost_at_ms = ppm->config.time_ms(ppm->config.hal_ctx);
        UCSI_LOG_I(ppm, "vbus lost, waiting %u ms for it to return", (unsigned)UCSI_PPM_TC_SRC_RECOVER_MS);
    }
}

// Feeds a raw CC reading into the debouncer. Nothing acts on it here — the
// value only becomes visible as tc_last_bc_lvl once tc_tick has seen it hold
// still, which is also where any consequence (detach, sink current limit) is
// drawn from.
static void tc_bc_lvl_observe(UcsiPpm* ppm, UcsiPpmPhyBcLvl bc_lvl) {
    if(bc_lvl.cc_busy) {
        // Mid-frame sample: tells us nothing about the termination. Drop it
        // and let whatever candidate was already running keep its timestamp.
        return;
    }
    if(bc_lvl.level == ppm->tc_last_bc_lvl) {
        ppm->tc_bc_lvl_pending_valid = false;
        return;
    }
    if(ppm->tc_bc_lvl_pending_valid && ppm->tc_bc_lvl_pending == bc_lvl.level) {
        return; // same candidate still maturing, keep the original timestamp
    }
    ppm->tc_bc_lvl_pending = bc_lvl.level;
    ppm->tc_bc_lvl_pending_ms = ppm->config.time_ms(ppm->config.hal_ctx);
    ppm->tc_bc_lvl_pending_valid = true;
}

static void handle_bc_lvl_changed(UcsiPpm* ppm, const UcsiPpmPhyEvent* event) {
    tc_bc_lvl_observe(ppm, event->u.bc_lvl);

    // Source-side detach still reacts to the raw reading: as a source we are
    // driving Rp and not receiving, so the line is quiet and the sample is
    // good. The sink path goes through the debouncer above instead.
    if(ppm->tc_state != (int)UcsiPpmTcStateAttachedSrc) return;
    if(event->u.bc_lvl.cc_busy) return;
    const uint8_t bc_lvl = event->u.bc_lvl.level;
    // Source-side detach detection. Two valid "partner gone" signatures:
    //   • BC_LVL = 0b11 — Rd removed, CC pulls up to the Rp source rail
    //   • BC_LVL = 0b00 — CC sits in vRa range (Type-A→C adapter / cable
    //     plug only — no real Rd-presenting sink). Common with USB-A
    //     accessories plugged via passive adapters; the chip momentarily
    //     settles SRC during DRP toggle on a CC glitch and then steady-
    //     state BC_LVL exposes the missing Rd.
    if(bc_lvl == UCSI_PPM_TC_BC_LVL_NO_RD || bc_lvl == UCSI_PPM_TC_BC_LVL_RA) {
        tc_enter_unattached(ppm, "src detach: bc_lvl says no Rd");
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
        tc_enter_unattached(ppm, "src detach: comp above mdac");
    }
}

// Rebuilds the whole chip configuration for the role we are already in,
// without touching the connector state machine.
//
// A Hard Reset leaves the FUSB302 BMC receiver in a state it does not come
// out of on its own: bring-up traces show the chip reporting I_ACTIVITY for
// an incoming frame that then never completes, for as long as we keep
// retrying — and recovering instantly the moment a SW_RESET happens to run.
// PD R3.0 §6.8.2 lets us treat the PHY as reset here, so do it deliberately
// rather than waiting for a detach to do it by accident.
static void tc_rearm_phy_after_hard_reset(UcsiPpm* ppm) {
    const bool attached = ppm->tc_state == (int)UcsiPpmTcStateAttachedSrc ||
                          ppm->tc_state == (int)UcsiPpmTcStateAttachedSnk;
    if(!attached) return;

    UCSI_LOG_I(ppm, "hard reset: re-arming phy for %s", tc_state_str(ppm->tc_state));

    // SW_RESET + POWER + masks. Also leaves CONTROL2.TOGGLE clear, so the
    // toggle FSM stays off — we are still attached and must not re-toggle.
    (void)ucsi_ppm_phy_init(ppm);

    // SWITCHES0 comes back as PDWN1|PDWN2, which is already what a sink
    // needs; both roles still need the measured/transmit CC pin selected.
    (void)ucsi_ppm_phy_lock_polarity(ppm, (UcsiPpmPhyCc)ppm->tc_orientation);
    if(ppm->tc_role_is_src) {
        (void)ucsi_ppm_phy_set_rp_current(ppm, ppm->config.source_rp_current);
        (void)ucsi_ppm_phy_set_source_termination(ppm, (UcsiPpmPhyCc)ppm->tc_orientation);
    }

    (void)ucsi_ppm_phy_enable_pd(ppm, UCSI_PPM_TC_PD_RETRIES);
    ucsi_ppm_phy_log_config(ppm, "pd re-armed");
}

void ucsi_ppm_tc_handle_phy_event(UcsiPpm* ppm, const UcsiPpmPhyEvent* event) {
    // Cheap and decisive when chasing a spurious detach: the raw chip event
    // next to the state it lands in.
    UCSI_LOG_D(ppm, "phy event %d in %s", (int)event->kind, tc_state_str(ppm->tc_state));

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
    case UcsiPpmPhyEventHardResetRx:
    case UcsiPpmPhyEventHardResetSent:
        // TC owns the chip's role configuration, so the PHY rebuild belongs
        // here. PRL flushes the FIFOs and PE restarts its state machine off
        // the same event — they run after us in the dispatch order.
        tc_rearm_phy_after_hard_reset(ppm);
        break;
    // PRL/PE events (MessageRx / TxSuccess / TxRetryFail / Collision) are
    // dispatched elsewhere.
    default:
        break;
    }
}

// Milliseconds left of `window` since `started_ms`, 0 once it has passed.
static uint32_t tc_remaining_ms(const UcsiPpm* ppm, uint32_t started_ms, uint32_t window_ms) {
    const uint32_t elapsed = (uint32_t)(ppm->config.time_ms(ppm->config.hal_ctx) - started_ms);
    return elapsed >= window_ms ? 0u : window_ms - elapsed;
}

uint32_t ucsi_ppm_tc_next_timeout_ms(const UcsiPpm* ppm) {
    if(ppm->tc_state == (int)UcsiPpmTcStateAttachWait) {
        const uint32_t debounce =
            tc_remaining_ms(ppm, ppm->tc_attach_wait_start_ms, UCSI_PPM_TC_CC_DEBOUNCE_MS);
        if(debounce) return debounce;
        // Past debounce the commit waits for VBUS, which arrives via the
        // I_VBUSOK interrupt — the only remaining deadline is the give-up.
        return tc_remaining_ms(ppm, ppm->tc_attach_wait_start_ms, UCSI_PPM_TC_ATTACH_WAIT_TIMEOUT_MS);
    }

    if(ppm->tc_state != (int)UcsiPpmTcStateAttachedSnk) return UCSI_PPM_NO_TIMEOUT;

    // Both sink-detach paths need a wakeup of their own; whichever is closer
    // wins, and tc_tick re-evaluates both.
    uint32_t next = UCSI_PPM_NO_TIMEOUT;
    if(ppm->tc_bc_lvl_pending_valid) {
        next = tc_remaining_ms(ppm, ppm->tc_bc_lvl_pending_ms, UCSI_PPM_TC_PD_DEBOUNCE_MS);
    }
    if(ppm->tc_vbus_lost) {
        const uint32_t vbus =
            tc_remaining_ms(ppm, ppm->tc_vbus_lost_at_ms, UCSI_PPM_TC_SRC_RECOVER_MS);
        if(vbus < next) next = vbus;
    }
    return next;
}

// Promotes a CC reading that has held still for tPDDebounce into
// tc_last_bc_lvl. Returns true if the debounced value changed.
static bool tc_bc_lvl_settle(UcsiPpm* ppm) {
    if(!ppm->tc_bc_lvl_pending_valid) return false;

    const uint32_t now = ppm->config.time_ms(ppm->config.hal_ctx);
    if((uint32_t)(now - ppm->tc_bc_lvl_pending_ms) < UCSI_PPM_TC_PD_DEBOUNCE_MS) return false;

    ppm->tc_last_bc_lvl = ppm->tc_bc_lvl_pending;
    ppm->tc_bc_lvl_pending_valid = false;
    UCSI_LOG_D(ppm, "cc level settled at %u", (unsigned)ppm->tc_last_bc_lvl);
    return true;
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
                tc_enter_unattached(ppm, "attachwait timeout, no vbus");
            }
        }
        return;
    }

    if(ppm->tc_state != (int)UcsiPpmTcStateAttachedSnk) return;
    if(ucsi_ppm_pe_pr_swap_in_progress(ppm)) return;

    // Keep the debounced CC level current whatever else happens — the sink
    // current limit is derived from it too.
    (void)tc_bc_lvl_settle(ppm);

    // VBUS is the detach signal, but it drops legitimately for the length of
    // a Hard Reset, so on its own it decides nothing.
    if(!ppm->tc_vbus_lost) return;

    // The source's Rp went with it: the cable is out, nothing to wait for.
    // Only the debounced level is trusted here — a raw sample taken while the
    // line carries PD traffic reads as "no Rp" and would tear down a healthy
    // connection mid-negotiation.
    if(ppm->tc_last_bc_lvl == UCSI_PPM_TC_BC_LVL_SNK_NO_RP) {
        tc_enter_unattached(ppm, "snk detach: vbus and Rp both gone");
        return;
    }

    // Rp still present, so the partner is mid-Hard-Reset. Give it tSrcRecover
    // to bring VBUS back before giving up on it.
    const uint32_t now = ppm->config.time_ms(ppm->config.hal_ctx);
    if((uint32_t)(now - ppm->tc_vbus_lost_at_ms) >= UCSI_PPM_TC_SRC_RECOVER_MS) {
        tc_enter_unattached(ppm, "snk detach: vbus never came back");
    }
}
