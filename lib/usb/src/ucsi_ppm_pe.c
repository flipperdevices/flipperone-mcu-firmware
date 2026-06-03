#include "ucsi_ppm_pe.h"

#include "ucsi_ppm_prl.h"
#include "ucsi_ppm_tc.h"

#include <string.h>

#include <furi.h>

// #define UCSI_PE_DEBUG_ENABLED

#define TAG "UcsiPe"

#ifdef UCSI_PE_DEBUG_ENABLED
#define UCSI_PE_WARN(...)  FURI_LOG_W(TAG, __VA_ARGS__)
#define UCSI_PE_ERROR(...) FURI_LOG_E(TAG, __VA_ARGS__)
#else
#define UCSI_PE_WARN(...)
#define UCSI_PE_ERROR(...)
#endif

// --- PD message types (PD R3.0 Table 6.4 / 6.5) ----------------------------

#define PD_MSG_TYPE_GOODCRC          0x01u
#define PD_MSG_TYPE_ACCEPT           0x03u
#define PD_MSG_TYPE_REJECT           0x04u
#define PD_MSG_TYPE_PS_RDY           0x06u
#define PD_MSG_TYPE_WAIT             0x0Cu
#define PD_MSG_TYPE_SOFT_RESET       0x0Du
#define PD_MSG_TYPE_NOT_SUPPORTED    0x10u
#define PD_MSG_TYPE_DR_SWAP          0x09u
#define PD_MSG_TYPE_PR_SWAP          0x0Au
#define PD_MSG_TYPE_VCONN_SWAP       0x0Bu
#define PD_MSG_TYPE_SOURCE_CAPS_DATA 0x01u // also 0x01, but distinguished by NDO>0
#define PD_MSG_TYPE_REQUEST_DATA     0x02u

// --- PD header layout (PD R3.0 §6.2.1.1) -----------------------------------

#define PD_HDR_MSG_TYPE_MASK  0x001Fu
#define PD_HDR_DATA_ROLE_BIT  (1u << 5)
#define PD_HDR_SPEC_REV_SHIFT 6u
#define PD_HDR_POWER_ROLE_BIT (1u << 8)
#define PD_HDR_NUM_OBJ_SHIFT  12u
#define PD_HDR_NUM_OBJ_MASK   (0x07u << PD_HDR_NUM_OBJ_SHIFT)

#define PD_SPEC_REV_3_0 0b10u

// --- Source Fixed PDO field layout (PD R3.0 Table 6-9) ---------------------

#define PDO_TYPE_SHIFT            30u
#define PDO_TYPE_MASK             (0x3u << PDO_TYPE_SHIFT)
#define PDO_TYPE_FIXED            0x0u
#define PDO_FIXED_VOLTAGE_SHIFT   10u
#define PDO_FIXED_VOLTAGE_MASK    (0x3FFu << PDO_FIXED_VOLTAGE_SHIFT)
#define PDO_FIXED_VOLTAGE_UNIT_MV 50u
#define PDO_FIXED_CURRENT_MASK    0x3FFu
#define PDO_FIXED_CURRENT_UNIT_MA 10u

// --- Request RDO layout (PD R3.0 §6.4.2 Fixed/Variable RDO) ----------------

#define RDO_OBJ_POSITION_SHIFT 28u
#define RDO_CAP_MISMATCH_BIT   (1u << 26)
#define RDO_USB_COMMS_BIT      (1u << 25)
#define RDO_NO_USB_SUSPEND_BIT (1u << 24)
#define RDO_OP_CURRENT_SHIFT   10u

// --- Timers (PD R3.0 §7.9) -------------------------------------------------

#define UCSI_PPM_PE_SINK_WAIT_CAP_MS   465u // tTypeCSinkWaitCap nominal
#define UCSI_PPM_PE_SENDER_RESPONSE_MS 500u // tSenderResponse max
#define UCSI_PPM_PE_PS_TRANSITION_MS   500u // tPSTransition SPR nominal
#define UCSI_PPM_PE_SOURCE_CAP_MS      150u // tTypeCSendSourceCap nominal
#define UCSI_PPM_PE_CAPS_COUNT_MAX     50u // nCapsCount
#define UCSI_PPM_PE_HARD_RESET_MAX     2u // nHardResetCount (PD R3.0 Table 7.12)
#define UCSI_PPM_PE_PS_SOURCE_OFF_MS   750u // tPSSourceOff (PR_Swap §6.6.x)
#define UCSI_PPM_PE_PS_SOURCE_ON_MS    480u // tPSSourceOn
// Empirical settle delay after the sink→source SWITCHES0 flip + OTG enable
// before the chip's FUSB302 BMC modulator reliably accepts the first source-
// role TX. Spec doesn't quote a value — picked low enough that VBUSOK
// debounce dominates, high enough that the post-flip NACK we observed in
// bring-up doesn't recur.
#define UCSI_PPM_PE_PR_SWAP_SRC_SETTLE_MS 50u

// --- helpers ---------------------------------------------------------------

static uint16_t pe_build_header(uint8_t msg_type, uint8_t num_objects, bool power_role_src, bool data_role_dfp) {
    uint16_t hdr = (uint16_t)(msg_type & PD_HDR_MSG_TYPE_MASK);
    if(data_role_dfp) hdr |= PD_HDR_DATA_ROLE_BIT;
    hdr |= (uint16_t)(PD_SPEC_REV_3_0 << PD_HDR_SPEC_REV_SHIFT);
    if(power_role_src) hdr |= PD_HDR_POWER_ROLE_BIT;
    hdr |= (uint16_t)(((uint32_t)num_objects & 0x07u) << PD_HDR_NUM_OBJ_SHIFT);
    return hdr;
}

static bool pdo_is_fixed(uint32_t pdo) {
    return ((pdo & PDO_TYPE_MASK) >> PDO_TYPE_SHIFT) == PDO_TYPE_FIXED;
}

static uint16_t pdo_fixed_voltage_mv(uint32_t pdo) {
    const uint32_t units = (pdo & PDO_FIXED_VOLTAGE_MASK) >> PDO_FIXED_VOLTAGE_SHIFT;
    return (uint16_t)(units * PDO_FIXED_VOLTAGE_UNIT_MV);
}

static uint16_t pdo_fixed_current_ma(uint32_t pdo) {
    const uint32_t units = pdo & PDO_FIXED_CURRENT_MASK;
    return (uint16_t)(units * PDO_FIXED_CURRENT_UNIT_MA);
}

static uint32_t pe_build_rdo(uint8_t obj_position, uint16_t operating_current_ma, bool usb_comms) {
    uint32_t rdo = 0u;
    rdo |= ((uint32_t)(obj_position & 0x07u) << RDO_OBJ_POSITION_SHIFT);
    if(usb_comms) rdo |= RDO_USB_COMMS_BIT;
    rdo |= RDO_NO_USB_SUSPEND_BIT; // PD-managed devices typically refuse USB suspend
    rdo |= ((uint32_t)((operating_current_ma / 10u) & 0x3FFu) << RDO_OP_CURRENT_SHIFT);
    rdo |= ((uint32_t)(operating_current_ma / 10u) & 0x3FFu); // max = op for simplicity
    return rdo;
}

static void pe_arm_timer(UcsiPpm* ppm) {
    ppm->pe_timer_start_ms = ppm->config.time_ms(ppm->config.hal_ctx);
}

static bool pe_timer_expired(const UcsiPpm* ppm, uint32_t timeout_ms) {
    const uint32_t now = ppm->config.time_ms(ppm->config.hal_ctx);
    return (uint32_t)(now - ppm->pe_timer_start_ms) >= timeout_ms;
}

// --- state-machine actions -------------------------------------------------

static void pe_to_error(UcsiPpm* ppm) {
    // Terminal state — only used for internal errors (config/I-O) or when
    // HardResetCounter exhausts. Recoverable protocol errors go through
    // pe_request_hard_reset instead.
    ppm->pe_state = (int)UcsiPpmPeStateError;
}

// Forward declarations (Soft Reset can escalate to Hard Reset on failure).
static void pe_request_hard_reset(UcsiPpm* ppm);
static void pe_restart_after_hard_reset(UcsiPpm* ppm);
static bool pe_send_control(UcsiPpm* ppm, uint8_t msg_type);

// Triggers a self-initiated Soft_Reset per PD R3.0 §6.3.13 / §8.3.3.4. Resets
// PRL state (so msg_id=0 goes out, partner's stale dedup state is cleared on
// arrival), transmits Soft_Reset, arms SenderResponseTimer waiting for Accept.
// Used when TX retries are exhausted — soft is the first escalation step
// before falling back to Hard Reset.
static void pe_request_soft_reset(UcsiPpm* ppm) {
    UCSI_PE_WARN("soft reset (state=%d)", ppm->pe_state);
    // PRL state must be reset *before* sending so the Soft_Reset frame goes
    // out with MessageID = 0 (§6.8.1.2).
    (void)ucsi_ppm_prl_reset(ppm);
    if(!pe_send_control(ppm, PD_MSG_TYPE_SOFT_RESET)) {
        // Couldn't even enqueue — escalate immediately.
        pe_request_hard_reset(ppm);
        return;
    }
    ppm->pe_state = (int)UcsiPpmPeWaitForSoftResetAccept;
    pe_arm_timer(ppm);
}

// Triggers a self-initiated Hard Reset: tells PHY to drive the BMC pattern
// and waits for the HARDSENT event. Bounded by nHardResetCount — past that
// we give up to Error per PD R3.0 §8.3.3.6.
static void pe_request_hard_reset(UcsiPpm* ppm) {
    UCSI_PE_WARN("hard reset (state=%d, counter=%u)", ppm->pe_state, ppm->pe_hard_reset_counter);
    if(ppm->pe_hard_reset_counter >= UCSI_PPM_PE_HARD_RESET_MAX) {
        UCSI_PE_ERROR("hard reset counter exhausted → Error");
        pe_to_error(ppm);
        return;
    }
    if(ucsi_ppm_phy_send_hard_reset(ppm) != UcsiPpmStatusOk) {
        pe_to_error(ppm);
        return;
    }
    ppm->pe_hard_reset_counter++;
    ppm->pe_state = (int)UcsiPpmPePendingHardResetSent;
    // No PE timer here — completion arrives as HardResetSent from the chip.
}

// Header for a PD message we originate. Power role tracks tc_role_is_src;
// data role tracks pe_data_role_is_dfp (so DR_Swap can flip data role
// without touching power role).
static uint16_t pe_make_header(const UcsiPpm* ppm, uint8_t msg_type, uint8_t num_objects) {
    return pe_build_header(msg_type, num_objects, ppm->tc_role_is_src, ppm->pe_data_role_is_dfp);
}

// Sends a Control (NDO=0) message via PRL. Returns true on enqueue success.
static bool pe_send_control(UcsiPpm* ppm, uint8_t msg_type) {
    UcsiPpmPhyPdMsg msg = {
        .sop_type = UcsiPpmPhySopTypeSop,
        .header = pe_make_header(ppm, msg_type, 0u),
        .object_count = 0u,
    };
    return ucsi_ppm_prl_send_message(ppm, &msg) == UcsiPpmStatusOk;
}

// Sends Source_Capabilities advertising config.source_caps PDOs.
static bool pe_src_send_caps(UcsiPpm* ppm) {
    const UcsiPpmPdoList* list = &ppm->config.source_caps;
    if(list->count == 0u) return false;

    UcsiPpmPhyPdMsg msg = {
        .sop_type = UcsiPpmPhySopTypeSop,
        .header = pe_make_header(ppm, PD_MSG_TYPE_SOURCE_CAPS_DATA, list->count),
        .object_count = list->count,
    };
    for(uint8_t i = 0; i < list->count; ++i) {
        msg.objects[i] = list->pdos[i];
    }
    return ucsi_ppm_prl_send_message(ppm, &msg) == UcsiPpmStatusOk;
}

// Picks PDO #1 from the received Source_Capabilities (mandatory vSafe5V Fixed
// per PD R3.0 §6.4.1), builds a matching RDO at the advertised max current,
// transmits Request, arms SenderResponseTimer.
static void pe_send_request(UcsiPpm* ppm) {
    if(ppm->pe_received_pdo_count == 0u) {
        pe_to_error(ppm);
        return;
    }
    const uint32_t pdo = ppm->pe_received_pdos[0];
    if(!pdo_is_fixed(pdo)) {
        pe_to_error(ppm);
        return;
    }
    const uint16_t max_current_ma = pdo_fixed_current_ma(pdo);
    const uint32_t rdo = pe_build_rdo(1u, max_current_ma, true);

    UcsiPpmPhyPdMsg msg = {
        .sop_type = UcsiPpmPhySopTypeSop,
        .header = pe_make_header(ppm, PD_MSG_TYPE_REQUEST_DATA, 1u),
        .object_count = 1u,
    };
    msg.objects[0] = rdo;

    const UcsiPpmStatus s = ucsi_ppm_prl_send_message(ppm, &msg);
    if(s != UcsiPpmStatusOk) {
        pe_to_error(ppm);
        return;
    }
    ppm->pe_requested_pdo_index = 1u;
    ppm->pe_current_rdo = rdo;
    ppm->pe_state = (int)UcsiPpmPeSnkWaitForAccept;
    pe_arm_timer(ppm);
}

// Source-side: partner sent a Request RDO. Validate the object position,
// send Accept, kick off PSU voltage transition.
static void pe_src_handle_request(UcsiPpm* ppm, const UcsiPpmPhyPdMsg* msg) {
    if(msg->object_count != 1u) return; // malformed — ignore in v1

    const uint32_t rdo = msg->objects[0];
    const uint8_t obj_pos = (uint8_t)((rdo >> RDO_OBJ_POSITION_SHIFT) & 0x07u);
    if(obj_pos == 0u || obj_pos > ppm->config.source_caps.count) {
        // Out of range — Reject and bail to error (v1; spec-rich version would
        // stay in SrcSendCapabilities and continue advertising).
        (void)pe_send_control(ppm, PD_MSG_TYPE_REJECT);
        pe_to_error(ppm);
        return;
    }

    const uint32_t pdo = ppm->config.source_caps.pdos[obj_pos - 1u];
    if(!pdo_is_fixed(pdo)) {
        // We only advertise Fixed PDOs in v1; anything else is a bug.
        (void)pe_send_control(ppm, PD_MSG_TYPE_REJECT);
        pe_to_error(ppm);
        return;
    }
    const uint16_t voltage_mv = pdo_fixed_voltage_mv(pdo);
    const uint16_t current_ma = pdo_fixed_current_ma(pdo);

    // Cache the agreed contract so PS_RDY commits the right values.
    ppm->pe_requested_pdo_index = obj_pos;
    ppm->pe_current_rdo = rdo;
    ppm->pe_negotiated_voltage_mv = voltage_mv;
    ppm->pe_negotiated_current_ma = current_ma;

    if(!pe_send_control(ppm, PD_MSG_TYPE_ACCEPT)) {
        pe_to_error(ppm);
        return;
    }

    // Tell external PSU to ramp; caller signals completion via
    // ucsi_ppm_notify_power_supply_ready (api.md §5.4).
    (void)ppm->config.power_supply_set(ppm->config.hal_ctx, voltage_mv, current_ma);

    ppm->pe_state = (int)UcsiPpmPeSrcTransitionSupply;
    pe_arm_timer(ppm); // PSTransitionTimer
}

static void pe_commit_contract(UcsiPpm* ppm) {
    // Voltage comes from the selected PDO; operating current comes from the
    // RDO we actually sent (bits 19:10, 10 mA units). These diverge after a
    // SET_POWER_LEVEL renegotiate — the PDO advertises max while the contract
    // is for a lower op_current.
    const uint8_t idx = ppm->pe_requested_pdo_index;
    if(idx == 0u || idx > ppm->pe_received_pdo_count) {
        pe_to_error(ppm);
        return;
    }
    const uint32_t pdo = ppm->pe_received_pdos[idx - 1u];
    ppm->pe_negotiated_voltage_mv = pdo_fixed_voltage_mv(pdo);
    const uint16_t op_units = (uint16_t)((ppm->pe_current_rdo >> RDO_OP_CURRENT_SHIFT) & 0x3FFu);
    ppm->pe_negotiated_current_ma = (uint16_t)(op_units * PDO_FIXED_CURRENT_UNIT_MA);
    ppm->pe_state = (int)UcsiPpmPeSnkReady;
    ppm->pe_hard_reset_counter = 0u; // clean contract — fresh budget
    // Let OPM observe the new PD contract via GET_CONNECTOR_STATUS.
    ucsi_ppm_notify_connector_change(ppm, UCSI_PPM_CSC_POWER_OP_MODE_CHANGE | UCSI_PPM_CSC_NEGOTIATED_PL_CHANGE);
}

// --- message dispatcher ----------------------------------------------------

static void pe_handle_data_message(UcsiPpm* ppm, const UcsiPpmPhyPdMsg* msg) {
    const uint8_t type = (uint8_t)(msg->header & PD_HDR_MSG_TYPE_MASK);

    // Source_Capabilities arriving at sink-side WaitForCapabilities.
    if(ppm->pe_state == (int)UcsiPpmPeSnkWaitForCapabilities && type == PD_MSG_TYPE_SOURCE_CAPS_DATA) {
        ppm->pe_received_pdo_count = (uint8_t)(msg->object_count <= UCSI_PPM_MAX_PDOS ? msg->object_count : UCSI_PPM_MAX_PDOS);
        for(uint8_t i = 0; i < ppm->pe_received_pdo_count; ++i) {
            ppm->pe_received_pdos[i] = msg->objects[i];
        }
        pe_send_request(ppm);
        return;
    }

    // Request arriving at source-side SrcSendCapabilities.
    if(ppm->pe_state == (int)UcsiPpmPeSrcSendCapabilities && type == PD_MSG_TYPE_REQUEST_DATA) {
        pe_src_handle_request(ppm, msg);
        return;
    }

    // Other data messages — ignored in v1.
}

static bool pe_in_ready(const UcsiPpm* ppm) {
    return ppm->pe_state == (int)UcsiPpmPeSnkReady || ppm->pe_state == (int)UcsiPpmPeSrcReady;
}

static void pe_handle_control_message(UcsiPpm* ppm, uint8_t type) {
    // Incoming Soft_Reset is processed regardless of current state — PRL has
    // already reset MessageID counters on detecting the SOFT_RESET frame
    // (§6.8.1.2); we Accept and restart contract negotiation.
    if(type == PD_MSG_TYPE_SOFT_RESET) {
        if(!pe_send_control(ppm, PD_MSG_TYPE_ACCEPT)) {
            pe_request_hard_reset(ppm);
            return;
        }
        pe_restart_after_hard_reset(ppm);
        return;
    }

    // Swap requests are only processed from an established contract (*Ready).
    // PR_Swap / VCONN_Swap aren't implemented in v1 (require VBUS-handover /
    // VCONN HAL hooks that don't exist yet) — we Reject per PD §6.3.x so the
    // partner can fall back. DR_Swap is fully wired: gated by accept_dr_swap
    // policy, Accept flips data role on both ends.
    if(type == PD_MSG_TYPE_DR_SWAP) {
        if(!pe_in_ready(ppm)) return;
        if(!ppm->accept_dr_swap) {
            (void)pe_send_control(ppm, PD_MSG_TYPE_REJECT);
            return;
        }
        if(!pe_send_control(ppm, PD_MSG_TYPE_ACCEPT)) {
            pe_request_hard_reset(ppm);
            return;
        }
        // PD §6.3.10: data role flips after Accept is on the wire.
        ppm->pe_data_role_is_dfp = !ppm->pe_data_role_is_dfp;
        ucsi_ppm_notify_connector_change(ppm, UCSI_PPM_CSC_PARTNER_CHANGED);
        return;
    }
    if(type == PD_MSG_TYPE_PR_SWAP || type == PD_MSG_TYPE_VCONN_SWAP) {
        if(!pe_in_ready(ppm)) return;
        (void)pe_send_control(ppm, PD_MSG_TYPE_REJECT);
        return;
    }

    switch(ppm->pe_state) {
    case(int)UcsiPpmPeSnkWaitForAccept:
        if(type == PD_MSG_TYPE_ACCEPT) {
            ppm->pe_state = (int)UcsiPpmPeSnkWaitForPsRdy;
            pe_arm_timer(ppm);
        } else if(type == PD_MSG_TYPE_REJECT || type == PD_MSG_TYPE_WAIT) {
            // Reject / Wait: per PD §8.3.3.3 sink falls into PE_SNK_Hard_Reset.
            // A spec-rich PE would defer-and-retry on Wait; v1 collapses both.
            pe_request_hard_reset(ppm);
        }
        break;
    case(int)UcsiPpmPeSnkWaitForPsRdy:
        if(type == PD_MSG_TYPE_PS_RDY) {
            pe_commit_contract(ppm);
        }
        break;
    case(int)UcsiPpmPeWaitForSoftResetAccept:
        if(type == PD_MSG_TYPE_ACCEPT) {
            // Soft reset acknowledged — restart contract negotiation from
            // wait-caps / send-caps (same as post-Hard-Reset).
            pe_restart_after_hard_reset(ppm);
        } else if(type == PD_MSG_TYPE_REJECT || type == PD_MSG_TYPE_WAIT) {
            pe_request_hard_reset(ppm);
        }
        break;
    case(int)UcsiPpmPeWaitForDrSwapResponse:
        if(type == PD_MSG_TYPE_ACCEPT) {
            ppm->pe_data_role_is_dfp = !ppm->pe_data_role_is_dfp;
            ucsi_ppm_notify_connector_change(ppm, UCSI_PPM_CSC_PARTNER_CHANGED);
        }
        // Accept / Reject / Wait / Not_Supported all return to *Ready —
        // only Accept flips the role. Partner can refuse via Reject or
        // Wait (PD §8.3.3.8) or declare DR_Swap unsupported (PD §6.5);
        // either way we just stay as we were.
        if(type == PD_MSG_TYPE_ACCEPT || type == PD_MSG_TYPE_REJECT || type == PD_MSG_TYPE_WAIT || type == PD_MSG_TYPE_NOT_SUPPORTED) {
            ppm->pe_state = ppm->tc_role_is_src ? (int)UcsiPpmPeSrcReady : (int)UcsiPpmPeSnkReady;
        }
        break;
    case(int)UcsiPpmPePrSwapSnkSendSwap:
        if(type == PD_MSG_TYPE_ACCEPT) {
            // Partner committed to the swap. Now wait for partner's PS_RDY
            // signalling its VBUS has dropped to vSafe0V.
            ppm->pe_state = (int)UcsiPpmPePrSwapSnkWaitForSourceOff;
            pe_arm_timer(ppm);
        } else if(
            type == PD_MSG_TYPE_REJECT || type == PD_MSG_TYPE_WAIT ||
            type == PD_MSG_TYPE_NOT_SUPPORTED) {
            // Partner refused — contract stays intact, we go back to SnkReady.
            ppm->pe_state = (int)UcsiPpmPeSnkReady;
        }
        break;
    case(int)UcsiPpmPePrSwapSnkWaitForSourceOff:
        if(type == PD_MSG_TYPE_PS_RDY) {
            // Partner's VBUS is now off. Commit the role flip and bring our
            // VBUS up. tc_role_is_src must flip BEFORE handle_vbus_changed
            // sees the partner's VBUS=0 (which it will any moment now via
            // I_VBUSOK), otherwise the AttachedSnk detach handler would
            // tear us down. We also flip tc_state so the CC handlers and
            // GET_CONNECTOR_STATUS reflect the new role.
            ppm->tc_role_is_src = true;
            ppm->tc_state = (int)UcsiPpmTcStateAttachedSrc;
            (void)ucsi_ppm_phy_set_rp_current(ppm, ppm->config.source_rp_current);
            (void)ucsi_ppm_phy_set_source_termination(
                ppm, (UcsiPpmPhyCc)ppm->tc_orientation);
            ppm->config.gpio_write_vbus_source(ppm->config.hal_ctx, true);
            // Drop the old sink contract — we'll start fresh as source.
            ppm->pe_received_pdo_count = 0u;
            ppm->pe_requested_pdo_index = 0u;
            ppm->pe_current_rdo = 0u;
            ppm->pe_negotiated_voltage_mv = 0u;
            ppm->pe_negotiated_current_ma = 0u;
            ppm->pe_state = (int)UcsiPpmPePrSwapSnkSourceOn;
            pe_arm_timer(ppm);
        }
        break;
    default:
        break;
    }
}

// --- public API ------------------------------------------------------------

UcsiPpmStatus ucsi_ppm_pe_init(UcsiPpm* ppm) {
    ppm->pe_state = (int)UcsiPpmPeStateIdle;
    ppm->pe_timer_start_ms = 0u;
    memset(ppm->pe_received_pdos, 0, sizeof(ppm->pe_received_pdos));
    ppm->pe_received_pdo_count = 0u;
    ppm->pe_requested_pdo_index = 0u;
    ppm->pe_negotiated_voltage_mv = 0u;
    ppm->pe_negotiated_current_ma = 0u;
    ppm->pe_caps_counter = 0u;
    ppm->pe_current_rdo = 0u;
    ppm->pe_hard_reset_counter = 0u;
    ppm->pe_data_role_is_dfp = false;
    return UcsiPpmStatusOk;
}

UcsiPpmStatus ucsi_ppm_pe_reset(UcsiPpm* ppm) {
    return ucsi_ppm_pe_init(ppm);
}

void ucsi_ppm_pe_on_attach_snk(UcsiPpm* ppm) {
    (void)ucsi_ppm_pe_init(ppm);
    // Type-C convention: sink is UFP, source is DFP at attach. DR_Swap can
    // later flip this independently of the power role.
    ppm->pe_data_role_is_dfp = false;
    ppm->pe_state = (int)UcsiPpmPeSnkWaitForCapabilities;
    pe_arm_timer(ppm);
}

void ucsi_ppm_pe_on_attach_src(UcsiPpm* ppm) {
    (void)ucsi_ppm_pe_init(ppm);
    ppm->pe_data_role_is_dfp = true;
    ppm->pe_state = (int)UcsiPpmPeSrcSendCapabilities;
    if(!pe_src_send_caps(ppm)) {
        pe_to_error(ppm);
        return;
    }
    ppm->pe_caps_counter = 1u;
    pe_arm_timer(ppm); // SourceCapabilityTimer
}

void ucsi_ppm_pe_on_detach(UcsiPpm* ppm) {
    (void)ucsi_ppm_pe_init(ppm);
}

void ucsi_ppm_pe_on_power_supply_ready(UcsiPpm* ppm) {
    if(ppm->pe_state != (int)UcsiPpmPeSrcTransitionSupply) return;
    // PSU is at the requested voltage — tell partner the supply is ready and
    // commit to SrcReady. Contract values were latched in pe_src_handle_request.
    if(!pe_send_control(ppm, PD_MSG_TYPE_PS_RDY)) {
        pe_to_error(ppm);
        return;
    }
    ppm->pe_state = (int)UcsiPpmPeSrcReady;
    ppm->pe_hard_reset_counter = 0u;
    ucsi_ppm_notify_connector_change(ppm, UCSI_PPM_CSC_POWER_OP_MODE_CHANGE | UCSI_PPM_CSC_NEGOTIATED_PL_CHANGE);
}

void ucsi_ppm_pe_handle_message(UcsiPpm* ppm, const UcsiPpmPhyPdMsg* msg) {
    if(!msg || msg->sop_type != UcsiPpmPhySopTypeSop) return;
    if(ppm->pe_state == (int)UcsiPpmPeStateIdle || ppm->pe_state == (int)UcsiPpmPeStateError) {
        return;
    }

    if(msg->object_count > 0u) {
        pe_handle_data_message(ppm, msg);
    } else {
        const uint8_t type = (uint8_t)(msg->header & PD_HDR_MSG_TYPE_MASK);
        pe_handle_control_message(ppm, type);
    }
}

// Restarts the contract negotiation flow after a Hard Reset (ours or
// partner's). Sink waits for new Source_Capabilities; source resumes
// Source_Capabilities advertisement.
static void pe_restart_after_hard_reset(UcsiPpm* ppm) {
    // PRL counter is reset by the PRL handler that also sees the HardReset*
    // event — we just need to reposition the state machine.
    if(ppm->tc_role_is_src) {
        ppm->pe_state = (int)UcsiPpmPeSrcSendCapabilities;
        ppm->pe_caps_counter = 0u;
        if(!pe_src_send_caps(ppm)) {
            pe_to_error(ppm);
            return;
        }
        ppm->pe_caps_counter = 1u;
        pe_arm_timer(ppm);
    } else {
        ppm->pe_state = (int)UcsiPpmPeSnkWaitForCapabilities;
        pe_arm_timer(ppm);
    }
    // Drop any half-formed contract details — partner state is wiped.
    ppm->pe_requested_pdo_index = 0u;
    ppm->pe_current_rdo = 0u;
    ppm->pe_negotiated_voltage_mv = 0u;
    ppm->pe_negotiated_current_ma = 0u;
    ppm->pe_received_pdo_count = 0u;
}

UcsiPpmStatus ucsi_ppm_pe_request_renegotiate(UcsiPpm* ppm, uint16_t operating_current_ma) {
    // Only sink-side, only with a live explicit contract — otherwise we'd
    // have nothing to renegotiate against.
    if(ppm->pe_state != (int)UcsiPpmPeSnkReady) return UcsiPpmStatusInvalidArg;
    if(ppm->pe_received_pdo_count == 0u || ppm->pe_requested_pdo_index == 0u) {
        return UcsiPpmStatusInvalidArg;
    }
    const uint8_t pos = ppm->pe_requested_pdo_index;
    if(pos > ppm->pe_received_pdo_count) return UcsiPpmStatusInvalidArg;

    const uint32_t pdo = ppm->pe_received_pdos[pos - 1u];
    if(!pdo_is_fixed(pdo)) return UcsiPpmStatusInvalidArg;
    // Clamp to advertised max so we never request more than the source offers.
    const uint16_t max_ma = pdo_fixed_current_ma(pdo);
    const uint16_t req_ma = operating_current_ma > max_ma ? max_ma : operating_current_ma;

    const uint32_t rdo = pe_build_rdo(pos, req_ma, true);
    UcsiPpmPhyPdMsg msg = {
        .sop_type = UcsiPpmPhySopTypeSop,
        .header = pe_make_header(ppm, PD_MSG_TYPE_REQUEST_DATA, 1u),
        .object_count = 1u,
    };
    msg.objects[0] = rdo;

    if(ucsi_ppm_prl_send_message(ppm, &msg) != UcsiPpmStatusOk) {
        pe_to_error(ppm);
        return UcsiPpmStatusHalError;
    }
    ppm->pe_current_rdo = rdo;
    ppm->pe_state = (int)UcsiPpmPeSnkWaitForAccept;
    pe_arm_timer(ppm);
    return UcsiPpmStatusOk;
}

UcsiPpmStatus ucsi_ppm_pe_request_dr_swap(UcsiPpm* ppm, bool to_dfp) {
    if(!pe_in_ready(ppm)) return UcsiPpmStatusInvalidArg;
    if(ppm->pe_data_role_is_dfp == to_dfp) {
        // Already in requested role — nothing to do.
        return UcsiPpmStatusOk;
    }
    if(!pe_send_control(ppm, PD_MSG_TYPE_DR_SWAP)) {
        return UcsiPpmStatusHalError;
    }
    ppm->pe_state = (int)UcsiPpmPeWaitForDrSwapResponse;
    pe_arm_timer(ppm);
    return UcsiPpmStatusOk;
}

UcsiPpmStatus ucsi_ppm_pe_request_pr_swap_to_source(UcsiPpm* ppm) {
    // Only sink-to-source is wired in v1. Source-to-sink would mirror the
    // protocol but with us turning VBUS off first — needs more careful PSU
    // teardown ordering.
    if(ppm->pe_state != (int)UcsiPpmPeSnkReady) return UcsiPpmStatusInvalidArg;
    if(!pe_send_control(ppm, PD_MSG_TYPE_PR_SWAP)) {
        return UcsiPpmStatusHalError;
    }
    ppm->pe_state = (int)UcsiPpmPePrSwapSnkSendSwap;
    pe_arm_timer(ppm);
    return UcsiPpmStatusOk;
}

// True while a PR_Swap is mid-flight — used by TC to suppress the AttachedSnk
// "VBUS dropped → detach" path, since during PR_Swap the partner deliberately
// drops VBUS as part of the protocol.
bool ucsi_ppm_pe_pr_swap_in_progress(const UcsiPpm* ppm) {
    return ppm->pe_state == (int)UcsiPpmPePrSwapSnkSendSwap ||
           ppm->pe_state == (int)UcsiPpmPePrSwapSnkWaitForSourceOff ||
           ppm->pe_state == (int)UcsiPpmPePrSwapSnkSourceOn;
}

void ucsi_ppm_pe_handle_phy_event(UcsiPpm* ppm, const UcsiPpmPhyEvent* event) {
    switch(event->kind) {
    case UcsiPpmPhyEventHardResetSent:
        // Our Hard Reset just completed BMC transmission. Counter was
        // already incremented in pe_request_hard_reset.
        if(ppm->pe_state == (int)UcsiPpmPePendingHardResetSent) {
            pe_restart_after_hard_reset(ppm);
        }
        break;
    case UcsiPpmPhyEventHardResetRx:
        // Partner-initiated Hard Reset. Don't bump our counter (we didn't
        // send it); just restart the flow.
        if(ppm->pe_state != (int)UcsiPpmPeStateIdle && ppm->pe_state != (int)UcsiPpmPeStateError) {
            pe_restart_after_hard_reset(ppm);
        }
        break;
    case UcsiPpmPhyEventTxRetryFail:
        // PD R3.0 §6.7.1: TX retry exhausted → escalate. From an active
        // contract or mid-negotiation, the first step is Soft Reset; if the
        // failure happens during Soft Reset itself, go straight to Hard Reset.
        switch(ppm->pe_state) {
        case(int)UcsiPpmPeSnkReady:
        case(int)UcsiPpmPeSrcReady:
        case(int)UcsiPpmPeSnkWaitForAccept:
        case(int)UcsiPpmPeSnkWaitForPsRdy:
        case(int)UcsiPpmPeSrcSendCapabilities:
        case(int)UcsiPpmPeSrcTransitionSupply:
        case(int)UcsiPpmPeWaitForDrSwapResponse:
            pe_request_soft_reset(ppm);
            break;
        case(int)UcsiPpmPeWaitForSoftResetAccept:
            pe_request_hard_reset(ppm);
            break;
        default:
            // Idle / WaitForCapabilities / PendingHardResetSent / Error —
            // either nothing was in flight, or recovery is already running.
            break;
        }
        break;
    default:
        break;
    }
}

void ucsi_ppm_pe_tick(UcsiPpm* ppm) {
    switch(ppm->pe_state) {
    case(int)UcsiPpmPeSnkWaitForCapabilities:
        if(pe_timer_expired(ppm, UCSI_PPM_PE_SINK_WAIT_CAP_MS)) pe_request_hard_reset(ppm);
        break;
    case(int)UcsiPpmPeSnkWaitForAccept:
        if(pe_timer_expired(ppm, UCSI_PPM_PE_SENDER_RESPONSE_MS)) pe_request_hard_reset(ppm);
        break;
    case(int)UcsiPpmPeWaitForSoftResetAccept:
        // SenderResponseTimer also gates Soft_Reset Accept (PD §8.3.3.4).
        // No Accept in time → Hard Reset.
        if(pe_timer_expired(ppm, UCSI_PPM_PE_SENDER_RESPONSE_MS)) pe_request_hard_reset(ppm);
        break;
    case(int)UcsiPpmPeWaitForDrSwapResponse:
        // Same SenderResponseTimer (§6.6.2). On timeout PD §8.3.3.8 takes
        // the initiator to PE_*_Hard_Reset.
        if(pe_timer_expired(ppm, UCSI_PPM_PE_SENDER_RESPONSE_MS)) pe_request_hard_reset(ppm);
        break;
    case(int)UcsiPpmPePrSwapSnkSendSwap:
        if(pe_timer_expired(ppm, UCSI_PPM_PE_SENDER_RESPONSE_MS)) pe_request_hard_reset(ppm);
        break;
    case(int)UcsiPpmPePrSwapSnkWaitForSourceOff:
        // Partner has tPSSourceOff (~750 ms) to drop its VBUS and send
        // PS_RDY. Missing PS_RDY → Hard Reset per PD §6.6.x.
        if(pe_timer_expired(ppm, UCSI_PPM_PE_PS_SOURCE_OFF_MS)) pe_request_hard_reset(ppm);
        break;
    case(int)UcsiPpmPePrSwapSnkSourceOn: {
        // Wait for VBUSOK AND a settle window. The FUSB302 ends up in a
        // half-broken state after the sink→source SWITCHES0 flip — it
        // ACKs the address but NACKs FIFOS data writes. A full PD_RESET
        // alone wasn't enough; we need to wait for OTG ramp + chip to
        // re-acquire BMC framer state.
        bool vbus_ok = false;
        const uint32_t elapsed_ms =
            (uint32_t)(ppm->config.time_ms(ppm->config.hal_ctx) - ppm->pe_timer_start_ms);
        const bool ready = elapsed_ms >= UCSI_PPM_PE_PR_SWAP_SRC_SETTLE_MS &&
                           ucsi_ppm_phy_read_vbusok(ppm, &vbus_ok) == UcsiPpmStatusOk &&
                           vbus_ok;
        if(ready) {
            // Minimal post-PR_Swap re-arm: chip was in sink mode with AUTO_CRC
            // working, we only need to flip SWITCHES1.POWER_ROLE so any
            // auto-CRC the chip emits matches our new source role. The
            // termination flip already happened on PS_RDY (set_source_termination
            // covers SWITCHES0 + TX_CC in SWITCHES1). Avoid SW_RESET — it
            // tears down the working TX path and the chip NACKs the next
            // FIFOS push for reasons we don't fully understand (datasheet
            // doesn't spell out the post-reset TX prerequisites).
            (void)ucsi_ppm_phy_set_msg_header_bits(
                ppm, true /*src*/, ppm->pe_data_role_is_dfp, 0b10u /*PD R3.0*/);
            if(!pe_send_control(ppm, PD_MSG_TYPE_PS_RDY)) {
                pe_request_hard_reset(ppm);
                break;
            }
            // Do NOT push Source_Capabilities right after PS_RDY — the
            // FUSB302 BMC modulator is still serialising PS_RDY (~330 µs)
            // and NACKs the very next FIFOS push. Move to SrcSendCapabilities
            // with caps_counter=0 and let its 150 ms tick fire the first
            // caps transmission, by which point I_TXSENT for PS_RDY has long
            // since landed.
            ppm->pe_caps_counter = 0u;
            ppm->pe_state = (int)UcsiPpmPeSrcSendCapabilities;
            pe_arm_timer(ppm);
            ucsi_ppm_notify_connector_change(
                ppm, UCSI_PPM_CSC_POWER_DIRECTION_CHANGED | UCSI_PPM_CSC_PARTNER_CHANGED);
        } else if(pe_timer_expired(ppm, UCSI_PPM_PE_PS_SOURCE_ON_MS)) {
            // Our OTG didn't bring VBUS up in time — escalate.
            pe_request_hard_reset(ppm);
        }
        break;
    }
    case(int)UcsiPpmPeSnkWaitForPsRdy:
    case(int)UcsiPpmPeSrcTransitionSupply:
        if(pe_timer_expired(ppm, UCSI_PPM_PE_PS_TRANSITION_MS)) pe_request_hard_reset(ppm);
        break;
    case(int)UcsiPpmPeSrcSendCapabilities:
        // Resend Source_Capabilities periodically until partner replies with
        // a Request, capped at nCapsCount (partner is Type-C only after that).
        if(pe_timer_expired(ppm, UCSI_PPM_PE_SOURCE_CAP_MS)) {
            if(ppm->pe_caps_counter >= UCSI_PPM_PE_CAPS_COUNT_MAX) {
                pe_to_error(ppm);
            } else {
                if(!pe_src_send_caps(ppm)) {
                    pe_to_error(ppm);
                } else {
                    ppm->pe_caps_counter++;
                    pe_arm_timer(ppm);
                }
            }
        }
        break;
    default:
        break;
    }
}
