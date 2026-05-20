#include "ucsi_ppm_pe.h"

#include "ucsi_ppm_prl.h"

#include <string.h>

// --- PD message types (PD R3.0 Table 6.4 / 6.5) ----------------------------

#define PD_MSG_TYPE_GOODCRC             0x01u
#define PD_MSG_TYPE_ACCEPT              0x03u
#define PD_MSG_TYPE_REJECT              0x04u
#define PD_MSG_TYPE_PS_RDY              0x06u
#define PD_MSG_TYPE_WAIT                0x0Cu
#define PD_MSG_TYPE_SOURCE_CAPS_DATA    0x01u // also 0x01, but distinguished by NDO>0
#define PD_MSG_TYPE_REQUEST_DATA        0x02u

// --- PD header layout (PD R3.0 §6.2.1.1) -----------------------------------

#define PD_HDR_MSG_TYPE_MASK  0x001Fu
#define PD_HDR_DATA_ROLE_BIT  (1u << 5)
#define PD_HDR_SPEC_REV_SHIFT 6u
#define PD_HDR_POWER_ROLE_BIT (1u << 8)
#define PD_HDR_NUM_OBJ_SHIFT  12u
#define PD_HDR_NUM_OBJ_MASK   (0x07u << PD_HDR_NUM_OBJ_SHIFT)

#define PD_SPEC_REV_3_0       0b10u

// --- Source Fixed PDO field layout (PD R3.0 Table 6-9) ---------------------

#define PDO_TYPE_SHIFT          30u
#define PDO_TYPE_MASK           (0x3u << PDO_TYPE_SHIFT)
#define PDO_TYPE_FIXED          0x0u
#define PDO_FIXED_VOLTAGE_SHIFT 10u
#define PDO_FIXED_VOLTAGE_MASK  (0x3FFu << PDO_FIXED_VOLTAGE_SHIFT)
#define PDO_FIXED_VOLTAGE_UNIT_MV 50u
#define PDO_FIXED_CURRENT_MASK  0x3FFu
#define PDO_FIXED_CURRENT_UNIT_MA 10u

// --- Request RDO layout (PD R3.0 §6.4.2 Fixed/Variable RDO) ----------------

#define RDO_OBJ_POSITION_SHIFT  28u
#define RDO_CAP_MISMATCH_BIT    (1u << 26)
#define RDO_USB_COMMS_BIT       (1u << 25)
#define RDO_NO_USB_SUSPEND_BIT  (1u << 24)
#define RDO_OP_CURRENT_SHIFT    10u

// --- Timers (PD R3.0 §7.9) -------------------------------------------------

#define UCSI_PPM_PE_SINK_WAIT_CAP_MS   465u // tTypeCSinkWaitCap nominal
#define UCSI_PPM_PE_SENDER_RESPONSE_MS 500u // tSenderResponse max
#define UCSI_PPM_PE_PS_TRANSITION_MS   500u // tPSTransition SPR nominal

// --- helpers ---------------------------------------------------------------

static uint16_t pe_build_header(
    uint8_t msg_type,
    uint8_t num_objects,
    bool power_role_src,
    bool data_role_dfp) {
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

static uint32_t pe_build_rdo(
    uint8_t obj_position,
    uint16_t operating_current_ma,
    bool usb_comms) {
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
    ppm->pe_state = (int)UcsiPpmPeStateError;
    // TODO PE-4: enter Hard Reset path instead of stopping here.
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
        .header =
            pe_build_header(PD_MSG_TYPE_REQUEST_DATA, 1u, false /*sink*/, false /*UFP*/),
        .object_count = 1u,
    };
    msg.objects[0] = rdo;

    const UcsiPpmStatus s = ucsi_ppm_prl_send_message(ppm, &msg);
    if(s != UcsiPpmStatusOk) {
        pe_to_error(ppm);
        return;
    }
    ppm->pe_requested_pdo_index = 1u;
    ppm->pe_state = (int)UcsiPpmPeSnkWaitForAccept;
    pe_arm_timer(ppm);
}

static void pe_commit_contract(UcsiPpm* ppm) {
    // The PDO we requested has the negotiated voltage/current.
    const uint8_t idx = ppm->pe_requested_pdo_index;
    if(idx == 0u || idx > ppm->pe_received_pdo_count) {
        pe_to_error(ppm);
        return;
    }
    const uint32_t pdo = ppm->pe_received_pdos[idx - 1u];
    ppm->pe_negotiated_voltage_mv = pdo_fixed_voltage_mv(pdo);
    ppm->pe_negotiated_current_ma = pdo_fixed_current_ma(pdo);
    ppm->pe_state = (int)UcsiPpmPeSnkReady;
}

// --- message dispatcher ----------------------------------------------------

static void pe_handle_data_message(UcsiPpm* ppm, const UcsiPpmPhyPdMsg* msg) {
    const uint8_t type = (uint8_t)(msg->header & PD_HDR_MSG_TYPE_MASK);
    if(ppm->pe_state == (int)UcsiPpmPeSnkWaitForCapabilities &&
       type == PD_MSG_TYPE_SOURCE_CAPS_DATA) {
        // Source_Capabilities — cache the PDOs and respond with Request.
        ppm->pe_received_pdo_count =
            (uint8_t)(msg->object_count <= UCSI_PPM_MAX_PDOS ? msg->object_count :
                                                              UCSI_PPM_MAX_PDOS);
        for(uint8_t i = 0; i < ppm->pe_received_pdo_count; ++i) {
            ppm->pe_received_pdos[i] = msg->objects[i];
        }
        pe_send_request(ppm);
    }
    // Data messages outside of WaitForCapabilities (e.g. partner-initiated
    // Sink_Capabilities request, BIST, Vendor Defined) — ignored in v1.
}

static void pe_handle_control_message(UcsiPpm* ppm, uint8_t type) {
    switch(ppm->pe_state) {
    case (int)UcsiPpmPeSnkWaitForAccept:
        if(type == PD_MSG_TYPE_ACCEPT) {
            ppm->pe_state = (int)UcsiPpmPeSnkWaitForPsRdy;
            pe_arm_timer(ppm);
        } else if(type == PD_MSG_TYPE_REJECT || type == PD_MSG_TYPE_WAIT) {
            // Partner declined the request. v1 treats both terminally;
            // a richer PE would retry on Wait with a back-off (PD §8.3.3.5).
            pe_to_error(ppm);
        }
        break;
    case (int)UcsiPpmPeSnkWaitForPsRdy:
        if(type == PD_MSG_TYPE_PS_RDY) {
            pe_commit_contract(ppm);
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
    return UcsiPpmStatusOk;
}

UcsiPpmStatus ucsi_ppm_pe_reset(UcsiPpm* ppm) {
    return ucsi_ppm_pe_init(ppm);
}

void ucsi_ppm_pe_on_attach_snk(UcsiPpm* ppm) {
    (void)ucsi_ppm_pe_init(ppm);
    ppm->pe_state = (int)UcsiPpmPeSnkWaitForCapabilities;
    pe_arm_timer(ppm);
}

void ucsi_ppm_pe_on_detach(UcsiPpm* ppm) {
    (void)ucsi_ppm_pe_init(ppm);
}

void ucsi_ppm_pe_handle_message(UcsiPpm* ppm, const UcsiPpmPhyPdMsg* msg) {
    if(!msg || msg->sop_type != UcsiPpmPhySopTypeSop) return;
    if(ppm->pe_state == (int)UcsiPpmPeStateIdle ||
       ppm->pe_state == (int)UcsiPpmPeStateError) {
        return;
    }

    if(msg->object_count > 0u) {
        pe_handle_data_message(ppm, msg);
    } else {
        const uint8_t type = (uint8_t)(msg->header & PD_HDR_MSG_TYPE_MASK);
        pe_handle_control_message(ppm, type);
    }
}

void ucsi_ppm_pe_tick(UcsiPpm* ppm) {
    switch(ppm->pe_state) {
    case (int)UcsiPpmPeSnkWaitForCapabilities:
        if(pe_timer_expired(ppm, UCSI_PPM_PE_SINK_WAIT_CAP_MS)) pe_to_error(ppm);
        break;
    case (int)UcsiPpmPeSnkWaitForAccept:
        if(pe_timer_expired(ppm, UCSI_PPM_PE_SENDER_RESPONSE_MS)) pe_to_error(ppm);
        break;
    case (int)UcsiPpmPeSnkWaitForPsRdy:
        if(pe_timer_expired(ppm, UCSI_PPM_PE_PS_TRANSITION_MS)) pe_to_error(ppm);
        break;
    default:
        break;
    }
}
