#include "ucsi_ppm_pe.h"

#include "ucsi_ppm_prl.h"
#include "ucsi_ppm_tc.h"

#include <string.h>

#include <furi.h>

#define TAG "UcsiPe"

// --- PD message types (PD R3.0 Table 6.4 / 6.5) ----------------------------

#define PD_MSG_TYPE_GOODCRC          0x01u
#define PD_MSG_TYPE_PING             0x05u
#define PD_MSG_TYPE_ACCEPT           0x03u
#define PD_MSG_TYPE_REJECT           0x04u
#define PD_MSG_TYPE_PS_RDY           0x06u
#define PD_MSG_TYPE_WAIT             0x0Cu
#define PD_MSG_TYPE_SOFT_RESET       0x0Du
#define PD_MSG_TYPE_NOT_SUPPORTED    0x10u
#define PD_MSG_TYPE_DR_SWAP          0x09u
#define PD_MSG_TYPE_PR_SWAP          0x0Au
#define PD_MSG_TYPE_VCONN_SWAP       0x0Bu
#define PD_MSG_TYPE_GET_SOURCE_CAP   0x07u
#define PD_MSG_TYPE_GET_SINK_CAP     0x08u
#define PD_MSG_TYPE_GET_SOURCE_CAP_EXT 0x11u
#define PD_MSG_TYPE_GET_STATUS         0x12u
#define PD_MSG_TYPE_SOURCE_CAPS_DATA 0x01u // also 0x01, but distinguished by NDO>0
#define PD_MSG_TYPE_REQUEST_DATA     0x02u
#define PD_MSG_TYPE_SINK_CAPS_DATA   0x04u
#define PD_MSG_TYPE_VENDOR_DEFINED   0x0Fu

// Extended Message types (PD R3.2 Table 6.47). A namespace of its own — these
// numbers collide with Control and Data types and mean something else.
#define PD_EXT_MSG_TYPE_SOURCE_CAPS 0x01u
#define PD_EXT_MSG_TYPE_STATUS      0x02u

// Extended Message Header (PD R3.2 Table 6.48).
#define PD_EXT_HDR_CHUNKED_BIT     (1u << 15)
#define PD_EXT_HDR_DATA_SIZE_MASK  0x01FFu

// PD R3.2 Table 6.72. A chunk carries at most 26 payload bytes, which with the
// 2-byte extended header fills exactly 7 data objects — the most the 3-bit
// Number of Data Objects field can express.
#define PD_EXT_MAX_CHUNK_LEN 26u

// --- Structured VDM header layout (PD R3.0 Table 6-27) ---------------------

#define VDM_HDR_STRUCTURED_BIT   (1u << 15)
#define VDM_HDR_CMD_TYPE_SHIFT   6u
#define VDM_HDR_CMD_TYPE_MASK    (0x3u << VDM_HDR_CMD_TYPE_SHIFT)
#define VDM_CMD_TYPE_REQ         0x0u
#define VDM_CMD_TYPE_NAK         0x2u

// --- PD header layout (PD R3.0 §6.2.1.1) -----------------------------------

#define PD_HDR_MSG_TYPE_MASK  0x001Fu
#define PD_HDR_EXTENDED_BIT   (1u << 15)
#define PD_HDR_DATA_ROLE_BIT  (1u << 5)
#define PD_HDR_SPEC_REV_SHIFT 6u
#define PD_HDR_POWER_ROLE_BIT (1u << 8)
#define PD_HDR_NUM_OBJ_SHIFT  12u
#define PD_HDR_NUM_OBJ_MASK   (0x07u << PD_HDR_NUM_OBJ_SHIFT)

// Revision values live in ucsi_ppm_i.h (UCSI_PPM_SPEC_REV_*) — PRL owns which
// one is current, PE only stamps it.

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

// PrSwapSnkSourceOn is the one PE state that has to poll the chip (VBUSOK
// over I2C — see ucsi_ppm_pe_tick for why the IRQ can't be trusted there),
// so next_timeout_ms asks the host to wake us this often while in it.
#define UCSI_PPM_PE_SOURCE_ON_POLL_MS 10u

// --- helpers ---------------------------------------------------------------

static uint16_t pe_build_header(uint8_t msg_type, uint8_t num_objects, bool power_role_src, bool data_role_dfp, uint8_t spec_rev) {
    uint16_t hdr = (uint16_t)(msg_type & PD_HDR_MSG_TYPE_MASK);
    if(data_role_dfp) hdr |= PD_HDR_DATA_ROLE_BIT;
    hdr |= (uint16_t)((uint32_t)(spec_rev & 0x3u) << PD_HDR_SPEC_REV_SHIFT);
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

static const char* pe_state_str(int state) {
    switch((UcsiPpmPeState)state) {
    case UcsiPpmPeStateIdle: return "Idle";
    case UcsiPpmPeSnkWaitForCapabilities: return "SnkWaitForCaps";
    case UcsiPpmPeSnkWaitForAccept: return "SnkWaitForAccept";
    case UcsiPpmPeSnkWaitForPsRdy: return "SnkWaitForPsRdy";
    case UcsiPpmPeSnkReady: return "SnkReady";
    case UcsiPpmPeSrcSendCapabilities: return "SrcSendCaps";
    case UcsiPpmPeSrcTransitionSupply: return "SrcTransitionSupply";
    case UcsiPpmPeSrcReady: return "SrcReady";
    case UcsiPpmPePendingHardResetSent: return "PendingHardResetSent";
    case UcsiPpmPeWaitForSoftResetAccept: return "WaitForSoftResetAccept";
    case UcsiPpmPeWaitForDrSwapResponse: return "WaitForDrSwapResponse";
    case UcsiPpmPePrSwapSnkSendSwap: return "PrSwapSnkSendSwap";
    case UcsiPpmPePrSwapSnkWaitForSourceOff: return "PrSwapSnkWaitSourceOff";
    case UcsiPpmPePrSwapSnkSourceOn: return "PrSwapSnkSourceOn";
    case UcsiPpmPeStateError: return "Error";
    }
    return "?";
}

// Single funnel for PE state changes — every transition shows up in the log
// with both ends named. Silent when the state does not actually change.
static void pe_set_state(UcsiPpm* ppm, int new_state) {
    if(ppm->pe_state == new_state) return;
    UCSI_LOG_I(ppm, "%s -> %s", pe_state_str(ppm->pe_state), pe_state_str(new_state));
    ppm->pe_state = new_state;
}

static void pe_arm_timer(UcsiPpm* ppm) {
    ppm->pe_timer_start_ms = ppm->config.time_ms(ppm->config.hal_ctx);
}

static bool pe_timer_expired(const UcsiPpm* ppm, uint32_t timeout_ms) {
    const uint32_t now = ppm->config.time_ms(ppm->config.hal_ctx);
    return (uint32_t)(now - ppm->pe_timer_start_ms) >= timeout_ms;
}

static uint32_t pe_timer_remaining_ms(const UcsiPpm* ppm, uint32_t timeout_ms) {
    const uint32_t now = ppm->config.time_ms(ppm->config.hal_ctx);
    const uint32_t elapsed = (uint32_t)(now - ppm->pe_timer_start_ms);
    return elapsed >= timeout_ms ? 0u : timeout_ms - elapsed;
}

// --- state-machine actions -------------------------------------------------

static void pe_to_error(UcsiPpm* ppm) {
    // Terminal state — only used for internal errors (config/I-O) or when
    // HardResetCounter exhausts. Recoverable protocol errors go through
    // pe_request_hard_reset instead.
    pe_set_state(ppm, (int)UcsiPpmPeStateError);
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
    UCSI_LOG_W(ppm, "soft reset (state=%d)", ppm->pe_state);
    // PRL state must be reset *before* sending so the Soft_Reset frame goes
    // out with MessageID = 0 (§6.8.1.2).
    (void)ucsi_ppm_prl_reset(ppm);
    if(!pe_send_control(ppm, PD_MSG_TYPE_SOFT_RESET)) {
        // Couldn't even enqueue — escalate immediately.
        pe_request_hard_reset(ppm);
        return;
    }
    pe_set_state(ppm, (int)UcsiPpmPeWaitForSoftResetAccept);
    pe_arm_timer(ppm);
}

// Triggers a self-initiated Hard Reset: tells PHY to drive the BMC pattern
// and waits for the HARDSENT event. Bounded by nHardResetCount — past that
// we give up to Error per PD R3.0 §8.3.3.6.
static void pe_request_hard_reset(UcsiPpm* ppm) {
    UCSI_LOG_W(ppm, "hard reset (state=%d, counter=%u)", ppm->pe_state, (unsigned)ppm->pe_hard_reset_counter);
    if(ppm->pe_hard_reset_counter >= UCSI_PPM_PE_HARD_RESET_MAX) {
        if(ppm->pe_state == (int)UcsiPpmPeSnkWaitForCapabilities) {
            // PD R3.0 §8.3.3.4.1: a sink that spends its whole Hard Reset
            // budget waiting for Source_Capabilities concludes the partner is
            // not PD capable and settles for Type-C power. Stay where we are
            // with the timer disarmed — Error would be wrong (a plain USB
            // host is a perfectly good power source), and hard-resetting a
            // Type-C-only source achieves nothing while costing it a VBUS
            // cycle. Rp still governs how much we may draw.
            UCSI_LOG_I(ppm, "partner is not PD capable, staying a type-c only sink");
            ppm->pe_typec_only = true;
            return;
        }
        UCSI_LOG_E(ppm, "hard reset counter exhausted → Error");
        pe_to_error(ppm);
        return;
    }
    if(ucsi_ppm_phy_send_hard_reset(ppm) != UcsiPpmStatusOk) {
        pe_to_error(ppm);
        return;
    }
    ppm->pe_hard_reset_counter++;
    pe_set_state(ppm, (int)UcsiPpmPePendingHardResetSent);
    // No PE timer here — completion arrives as HardResetSent from the chip.
}

// Header for a PD message we originate. Power role tracks tc_role_is_src;
// data role tracks pe_data_role_is_dfp (so DR_Swap can flip data role
// without touching power role).
static uint16_t pe_make_header(const UcsiPpm* ppm, uint8_t msg_type, uint8_t num_objects) {
    return pe_build_header(
        msg_type, num_objects, ppm->tc_role_is_src, ppm->pe_data_role_is_dfp, ppm->prl_our_spec_rev);
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

// Sends Sink_Capabilities advertising config.sink_caps PDOs. Answering
// Get_Sink_Cap is mandatory for a Sink (PD R3.0 §6.4.1.3) — a partner that
// asks and is refused has no way to learn what we can take.
static bool pe_send_sink_caps(UcsiPpm* ppm) {
    const UcsiPpmPdoList* list = &ppm->config.sink_caps;
    if(list->count == 0u) return false;

    UcsiPpmPhyPdMsg msg = {
        .sop_type = UcsiPpmPhySopTypeSop,
        .header = pe_make_header(ppm, PD_MSG_TYPE_SINK_CAPS_DATA, list->count),
        .object_count = list->count,
    };
    for(uint8_t i = 0; i < list->count; ++i) {
        msg.objects[i] = list->pdos[i];
    }
    return ucsi_ppm_prl_send_message(ppm, &msg) == UcsiPpmStatusOk;
}

// Sends an Extended Message whose Data Block fits in a single chunk.
//
// Multi-chunk is deliberately absent: sending a Data Block longer than
// MaxExtendedMsgChunkLen means holding it while the receiver asks for each
// following chunk with Request Chunk, which needs ChunkSenderResponseTimer and
// a retransmit buffer. Nothing we answer comes close — the largest is SCEDB at
// 25 bytes — so refuse loudly rather than grow that machinery on speculation
// or, worse, truncate.
static bool pe_send_extended(UcsiPpm* ppm, uint8_t ext_type, const uint8_t* data, uint16_t len) {
    if(len > PD_EXT_MAX_CHUNK_LEN) {
        UCSI_LOG_E(
            ppm,
            "extended msg type 0x%02X needs %u bytes, chunking not implemented",
            (unsigned)ext_type,
            (unsigned)len);
        return false;
    }

    uint8_t bytes[2u + PD_EXT_MAX_CHUNK_LEN] = {0};
    // PD R3.2 §6.5.1.1: with Unchunked Extended Messages Supported clear in our
    // PDOs — which it is — the pair is chunked-only and the Chunked bit Shall be
    // set in every Extended Message, single chunk or not.
    const uint16_t ext_hdr =
        (uint16_t)(PD_EXT_HDR_CHUNKED_BIT | ((uint32_t)len & PD_EXT_HDR_DATA_SIZE_MASK));
    bytes[0] = (uint8_t)(ext_hdr & 0xFFu);
    bytes[1] = (uint8_t)(ext_hdr >> 8);
    memcpy(&bytes[2], data, len);

    // Number of Data Objects counts the extended header and is padded to the
    // 4-byte boundary with zeros (Table 6.48, Data Size).
    const uint8_t words = (uint8_t)((2u + len + 3u) / 4u);

    UcsiPpmPhyPdMsg msg = {
        .sop_type = UcsiPpmPhySopTypeSop,
        .header = (uint16_t)(pe_make_header(ppm, ext_type, words) | PD_HDR_EXTENDED_BIT),
        .object_count = words,
    };
    for(uint8_t i = 0; i < words; ++i) {
        const uint8_t* b = &bytes[i * 4u];
        msg.objects[i] = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) |
                         ((uint32_t)b[3] << 24);
    }
    UCSI_LOG_I(
        ppm,
        "tx extended type=0x%02X size=%u ndo=%u",
        (unsigned)ext_type,
        (unsigned)len,
        (unsigned)words);
    return ucsi_ppm_prl_send_message(ppm, &msg) == UcsiPpmStatusOk;
}

// --- Source_Capabilities_Extended (PD R3.2 §6.5.2, Table 6.50) -------------
//
// Identity. These belong in UcsiPpmConfig rather than here — product identity
// is the platform's, not the protocol layer's — and they will have to move once
// SKEDB lands, because it carries the same three fields. Duplicated for now.
//
// The product PID, matching lib/tusb/usb_descriptors.c. Not the 0xF102 in
// furi_hal_otp.c: that is the bootloader, a separate device with no PD stack at
// all, so the two never describe the same thing at the same time.
#define SCEDB_VID 0x37C1u // 16 bit. USB-IF assigned Vendor ID.
#define SCEDB_PID 0xF101u // 16 bit. Vendor assigned Product ID.
// 32 bit, USB-IF assigned per product, and Shall match the XID in the CertStat
// VDO. Left zero because we have no assigned value and no CertStat VDO to
// contradict — an invented XID would claim someone else's certification.
#define SCEDB_XID 0x00000000u
#define SCEDB_FW_VERSION 0x01u // 8 bit, vendor defined, no range constraint.
#define SCEDB_HW_VERSION 0x01u // 8 bit, vendor defined, no range constraint.

// Byte 10, Voltage Regulation. Bits 1:0 load step (00b = 150 mA/us default,
// 01b = 500 mA/us), bit 2 IoC (0b = 25% default, 1b = 90%), bits 7:3 reserved.
#define SCEDB_VOLTAGE_REGULATION 0x00u
// Byte 11, Holdup Time. 0 = unsupported, else milliseconds the output stays in
// regulation after AC is removed. We have no AC input, so the field is moot.
#define SCEDB_HOLDUP_TIME 0x00u
// Byte 12, Compliance. Bit 0 LPS, bit 1 PS1, bit 2 PS2 — claims about an
// external power supply, which we are not.
#define SCEDB_COMPLIANCE 0x00u
// Byte 13, Touch Current. Bit 0 low touch current EPS, bit 1 ground pin
// present, bit 2 ground pin is protective earth. Bus-powered handheld: none.
#define SCEDB_TOUCH_CURRENT 0x00u
// Bytes 14..19, Peak Current 1..3, 16 bit each. Bits 4:0 percent overload in
// 10% steps (max 25), bits 10:5 overload period in 20 ms steps, bits 14:11
// duty cycle in 5% steps, bit 15 VBUS droop allowed. Zero means we permit no
// overload at all, which is the honest answer for a battery-fed OTG path.
#define SCEDB_PEAK_CURRENT 0x0000u
// Byte 20, Touch Temp. 0 = IEC 60950-1 (default), 1 = IEC 62368-1 TS1,
// 2 = TS2. Anything else is invalid and read as 0.
#define SCEDB_TOUCH_TEMP 0x00u
// Byte 21, Source Inputs. Bit 0 external supply present, bit 1 that supply is
// unconstrained (ignored when bit 0 is clear), bit 2 internal battery present.
// We source from the internal cell with nothing external behind it.
#define SCEDB_SOURCE_INPUTS 0x04u
// Byte 22, Batteries. Bits 3:0 count of fixed batteries (0..4 valid), bits 7:4
// hot swappable slots (0..4 valid). Must match Battery Info in SKEDB.
#define SCEDB_BATTERIES 0x01u
// Byte 24, EPR Source PDP Rating, added in r3.1. Only ever sent once we
// negotiate a revision that defines it — see SCEDB_LEN. An SPR-only Source
// Shall report 0 in it.
#define SCEDB_EPR_PDP 0x00u

// Byte 23 is the SPR Source PDP Rating in whole watts, valid 0..100. Derived
// rather than written down so it cannot drift away from what we actually
// advertise in source_caps.
static uint8_t scedb_spr_pdp_watts(const UcsiPpm* ppm) {
    uint32_t best_mw = 0u;
    for(uint8_t i = 0; i < ppm->config.source_caps.count; ++i) {
        const uint32_t pdo = ppm->config.source_caps.pdos[i];
        if(!pdo_is_fixed(pdo)) continue;
        const uint32_t mw =
            ((uint32_t)pdo_fixed_voltage_mv(pdo) * pdo_fixed_current_ma(pdo)) / 1000u;
        if(mw > best_mw) best_mw = mw;
    }
    const uint32_t watts = best_mw / 1000u;
    return (uint8_t)(watts > 100u ? 100u : watts);
}

// The r3.0 block is bytes 0..23; r3.1 appended EPR Source PDP Rating at 24.
// Length follows the negotiated revision. Sending the longer form to a partner
// we told we speak r3.0 would be advertising a field out of a revision we just
// disclaimed — §6.5's "Ignore bytes added by a later revision" is a receiver's
// obligation, not a licence for the sender to send them early.
#define SCEDB_LEN_R30 24u
#define SCEDB_LEN_R31 25u

static bool pe_send_source_caps_extended(UcsiPpm* ppm) {
    uint8_t b[SCEDB_LEN_R31] = {0};
    const uint16_t len = SCEDB_LEN_R30;
    b[0] = (uint8_t)(SCEDB_VID & 0xFFu);
    b[1] = (uint8_t)(SCEDB_VID >> 8);
    b[2] = (uint8_t)(SCEDB_PID & 0xFFu);
    b[3] = (uint8_t)(SCEDB_PID >> 8);
    b[4] = (uint8_t)(SCEDB_XID & 0xFFu);
    b[5] = (uint8_t)((SCEDB_XID >> 8) & 0xFFu);
    b[6] = (uint8_t)((SCEDB_XID >> 16) & 0xFFu);
    b[7] = (uint8_t)((SCEDB_XID >> 24) & 0xFFu);
    b[8] = SCEDB_FW_VERSION;
    b[9] = SCEDB_HW_VERSION;
    b[10] = SCEDB_VOLTAGE_REGULATION;
    b[11] = SCEDB_HOLDUP_TIME;
    b[12] = SCEDB_COMPLIANCE;
    b[13] = SCEDB_TOUCH_CURRENT;
    b[14] = (uint8_t)(SCEDB_PEAK_CURRENT & 0xFFu);
    b[15] = (uint8_t)(SCEDB_PEAK_CURRENT >> 8);
    b[16] = b[14];
    b[17] = b[15];
    b[18] = b[14];
    b[19] = b[15];
    b[20] = SCEDB_TOUCH_TEMP;
    b[21] = SCEDB_SOURCE_INPUTS;
    b[22] = SCEDB_BATTERIES;
    b[23] = scedb_spr_pdp_watts(ppm);
    if(len > SCEDB_LEN_R30) b[24] = SCEDB_EPR_PDP;
    return pe_send_extended(ppm, PD_EXT_MSG_TYPE_SOURCE_CAPS, b, len);
}

// --- Status (PD R3.2 §6.5.3, Table 6.51) -----------------------------------
//
// Byte 0, Internal Temp: 0 = feature unsupported, 1 = below 2 C, 2..255 = the
// temperature in whole C. The charger and the fuel gauge both measure one, but
// getting either here needs a HAL hook the core does not have, so we declare
// it unsupported rather than invent a number.
#define SDB_INTERNAL_TEMP 0x00u
// Byte 1, Present Input. Bit 0 reserved; bits 2:1 are 00b internally powered,
// 01b DC external present, 11b AC external present; bit 3 internal power from a
// battery; bit 4 internal power from something other than a battery.
#define SDB_PRESENT_INPUT_DC_EXTERNAL (1u << 1)
#define SDB_PRESENT_INPUT_BATTERY     (1u << 3)
// Byte 2, Present Battery Input, a bitmap of which batteries are supplying:
// bits 3:0 fixed batteries 0..3, bits 7:4 hot swappable 0..3. Reserved unless
// Present Input bit 3 is set, so it is only meaningful when we run off the cell.
#define SDB_PRESENT_BATTERY_FIXED0 (1u << 0)
// Byte 3, Event Flags: bit 1 OCP, bit 2 OTP, bit 3 OVP — each Shall be cleared
// once reported — and bit 4 current-limit mode, PPS only. We latch none of
// these yet, so nothing to report.
#define SDB_EVENT_FLAGS 0x00u
// Byte 4, Temperature Status. Bits 2:1: 00b not supported, 01b normal,
// 10b warning, 11b over-temperature. Kept at not-supported to agree with
// Internal Temp above; claiming "normal" without measuring would be a guess.
#define SDB_TEMPERATURE_STATUS 0x00u
// Byte 5, Power Status: reasons a Source is limiting power. "Sinks Shall set
// this field to 0", and as a Source our limit is the configured PDO set rather
// than any of the listed conditions.
#define SDB_POWER_STATUS 0x00u

// The r3.0 block is bytes 0..5; r3.1 appended Power State Change at 6. Same
// reasoning as SCEDB_LEN_R30 — length follows the negotiated revision.
#define SDB_LEN_R30 6u

static bool pe_send_status(UcsiPpm* ppm) {
    uint8_t b[SDB_LEN_R30] = {0};
    b[0] = SDB_INTERNAL_TEMP;
    // Which supply is actually carrying us right now. As a Sink with VBUS from
    // the partner that is external DC; otherwise we are running off the cell.
    if(!ppm->tc_role_is_src && ppm->pe_state == (int)UcsiPpmPeSnkReady) {
        b[1] = SDB_PRESENT_INPUT_DC_EXTERNAL;
        b[2] = 0u; // Reserved while Present Input bit 3 is clear.
    } else {
        b[1] = SDB_PRESENT_INPUT_BATTERY;
        b[2] = SDB_PRESENT_BATTERY_FIXED0;
    }
    b[3] = SDB_EVENT_FLAGS;
    b[4] = SDB_TEMPERATURE_STATUS;
    b[5] = SDB_POWER_STATUS;
    return pe_send_extended(ppm, PD_EXT_MSG_TYPE_STATUS, b, SDB_LEN_R30);
}

// Our own Sink capability at exactly this voltage, or 0 if we never published
// one. Requesting a voltage we did not advertise, or more current than we said
// we would draw at it, contradicts the Sink_Capabilities the partner may have
// asked us for.
static uint16_t pe_sink_allowance_ma(const UcsiPpm* ppm, uint16_t voltage_mv) {
    for(uint8_t i = 0; i < ppm->config.sink_caps.count; ++i) {
        const uint32_t snk = ppm->config.sink_caps.pdos[i];
        if(!pdo_is_fixed(snk)) continue;
        if(pdo_fixed_voltage_mv(snk) != voltage_mv) continue;
        return pdo_fixed_current_ma(snk);
    }
    return 0u;
}

// Chooses which Source_Capabilities entry to Request: the most power we are
// allowed to take. Object positions are 1-based; returns 0 only if the source
// published nothing we can use at all.
//
// Only Fixed PDOs are considered. Variable and Battery supplies do not
// guarantee a voltage, and Augmented (PPS) needs a different RDO and a periodic
// re-Request we do not implement — selecting one would promise behaviour we do
// not have. If none of the offered voltages appear in our own sink caps we fall
// back to position 1, which PD R3.0 §6.4.1 requires every Source to publish as
// vSafe5V Fixed.
static uint8_t pe_select_pdo(const UcsiPpm* ppm, uint16_t* out_current_ma) {
    uint8_t best_pos = 0u;
    uint32_t best_power_mw = 0u;

    for(uint8_t i = 0; i < ppm->pe_received_pdo_count; ++i) {
        const uint32_t pdo = ppm->pe_received_pdos[i];
        if(!pdo_is_fixed(pdo)) continue;

        const uint16_t voltage_mv = pdo_fixed_voltage_mv(pdo);
        const uint16_t allowed_ma = pe_sink_allowance_ma(ppm, voltage_mv);
        if(allowed_ma == 0u) continue;

        const uint16_t offered_ma = pdo_fixed_current_ma(pdo);
        const uint16_t current_ma = offered_ma < allowed_ma ? offered_ma : allowed_ma;
        const uint32_t power_mw = ((uint32_t)voltage_mv * current_ma) / 1000u;

        // Strictly greater, so an equal-power tie keeps the entry found first.
        // PD R3.0 §6.4.1.2 orders Fixed PDOs by ascending voltage, so that is
        // the lower one — less step-down for the charger, less heat.
        if(power_mw > best_power_mw) {
            best_power_mw = power_mw;
            best_pos = (uint8_t)(i + 1u);
            *out_current_ma = current_ma;
        }
    }

    if(best_pos == 0u && pdo_is_fixed(ppm->pe_received_pdos[0])) {
        best_pos = 1u;
        *out_current_ma = pdo_fixed_current_ma(ppm->pe_received_pdos[0]);
    }
    return best_pos;
}

// Picks the best offered PDO, builds a matching RDO, transmits Request, arms
// SenderResponseTimer.
static void pe_send_request(UcsiPpm* ppm) {
    if(ppm->pe_received_pdo_count == 0u) {
        pe_to_error(ppm);
        return;
    }

    uint16_t max_current_ma = 0u;
    const uint8_t pos = pe_select_pdo(ppm, &max_current_ma);
    if(pos == 0u) {
        pe_to_error(ppm);
        return;
    }

    const uint32_t pdo = ppm->pe_received_pdos[pos - 1u];
    const uint32_t rdo = pe_build_rdo(pos, max_current_ma, true);
    // The number we commit to draw, next to the raw object it came from —
    // taking more than the source offered is indistinguishable from the
    // source being weak unless both are on the record.
    UCSI_LOG_I(
        ppm,
        "request pdo%u %u mV %u mA (pdo=%08lX rdo=%08lX)",
        (unsigned)pos,
        (unsigned)pdo_fixed_voltage_mv(pdo),
        (unsigned)max_current_ma,
        (unsigned long)pdo,
        (unsigned long)rdo);

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
    ppm->pe_requested_pdo_index = pos;
    ppm->pe_current_rdo = rdo;
    pe_set_state(ppm, (int)UcsiPpmPeSnkWaitForAccept);
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

    pe_set_state(ppm, (int)UcsiPpmPeSrcTransitionSupply);
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
    pe_set_state(ppm, (int)UcsiPpmPeSnkReady);
    ppm->pe_hard_reset_counter = 0u; // clean contract — fresh budget
    // Let OPM observe the new PD contract via GET_CONNECTOR_STATUS.
    ucsi_ppm_notify_connector_change(ppm, UCSI_PPM_CSC_POWER_OP_MODE_CHANGE | UCSI_PPM_CSC_NEGOTIATED_PL_CHANGE);
}

// --- message dispatcher ----------------------------------------------------

// PD R3.0 §6.3.17: a PD 3.0 device answers every message it does not
// implement with Not_Supported. Silence is a protocol violation, not a
// polite no-op — the partner's SenderResponseTimer expires, it escalates to
// Soft_Reset and then Hard Reset, and the connection collapses in a loop.
// Talkative partners make this immediate: a phone asks Get_Sink_Cap,
// Get_Revision, Get_Status and sends VDMs within milliseconds of the
// contract.
// Declines a message we do not implement. Not_Supported only exists from PD
// R3.0 on — in R2.0 that opcode is Reserved, so a partner we have stepped down
// to R2.0 for gets Reject instead (PD R2.0 §6.3.9), which is the refusal it
// knows how to read.
static void pe_reply_not_supported(UcsiPpm* ppm, uint8_t type, bool extended) {
    const bool pd3 = ppm->prl_our_spec_rev >= UCSI_PPM_SPEC_REV_3_0;
    // The "extended" prefix matters in a trace: Control, Data and Extended
    // message types are three separate namespaces, so the same number means
    // three different things.
    UCSI_LOG_I(
        ppm,
        "%smsg type 0x%02X unsupported, replying %s",
        extended ? "extended " : "",
        (unsigned)type,
        pd3 ? "Not_Supported" : "Reject");
    (void)pe_send_control(ppm, pd3 ? PD_MSG_TYPE_NOT_SUPPORTED : PD_MSG_TYPE_REJECT);
}

// True for messages that must never draw a Not_Supported: replies to our own
// requests (answering them would ping-pong forever), the ones handled
// explicitly above, and Ping, which PD R3.0 §6.3.6 says needs no reply.
static bool pe_control_needs_reply(uint8_t type) {
    switch(type) {
    case PD_MSG_TYPE_GOODCRC:
    case PD_MSG_TYPE_ACCEPT:
    case PD_MSG_TYPE_REJECT:
    case PD_MSG_TYPE_WAIT:
    case PD_MSG_TYPE_PS_RDY:
    case PD_MSG_TYPE_NOT_SUPPORTED:
    case PD_MSG_TYPE_SOFT_RESET:
    case PD_MSG_TYPE_DR_SWAP:
    case PD_MSG_TYPE_PR_SWAP:
    case PD_MSG_TYPE_VCONN_SWAP:
    case PD_MSG_TYPE_PING:
        return false;
    default:
        return true;
    }
}

// Answers a Structured VDM we do not implement. PD R3.0 §6.4.4.3 wants the
// same header back with Command Type flipped to NAK — SVID, Structured VDM
// Version, Object Position and Command all preserved, so the initiator can
// match the response to its request. Not_Supported is the answer for a
// message the port does not implement at all, and a partner that gets it
// where a NAK belongs cannot tell "no, not this command" from "no PD 3.0
// here". Returns false if this was not a Structured VDM request, leaving the
// caller to fall back.
static bool pe_reply_vdm_nak(UcsiPpm* ppm, const UcsiPpmPhyPdMsg* msg) {
    if(msg->object_count == 0u) return false;

    // SOP' / SOP'' address the cable plug, not us. We never enable them on RX,
    // but if one ever arrives, silence is the only correct answer — a port
    // answering for a cable would be worse than not answering.
    if(msg->sop_type != UcsiPpmPhySopTypeSop) return true;

    const uint32_t vdm_hdr = msg->objects[0];
    if(!(vdm_hdr & VDM_HDR_STRUCTURED_BIT)) return false; // Unstructured: owes Not_Supported.

    // ACK / NAK / BUSY are responses. One arriving unsolicited answers a
    // request we never sent, and replying to it would ping-pong forever.
    const uint32_t cmd_type = (vdm_hdr & VDM_HDR_CMD_TYPE_MASK) >> VDM_HDR_CMD_TYPE_SHIFT;
    if(cmd_type != VDM_CMD_TYPE_REQ) return true;

    const uint32_t nak_hdr = (vdm_hdr & ~(uint32_t)VDM_HDR_CMD_TYPE_MASK) |
                             ((uint32_t)VDM_CMD_TYPE_NAK << VDM_HDR_CMD_TYPE_SHIFT);
    UCSI_LOG_I(
        ppm,
        "vdm cmd 0x%02X svid %04lX, replying NAK",
        (unsigned)(vdm_hdr & 0x1Fu),
        (unsigned long)(vdm_hdr >> 16));

    UcsiPpmPhyPdMsg reply = {
        .sop_type = UcsiPpmPhySopTypeSop,
        .header = pe_make_header(ppm, PD_MSG_TYPE_VENDOR_DEFINED, 1u),
        .object_count = 1u,
    };
    reply.objects[0] = nak_hdr;
    (void)ucsi_ppm_prl_send_message(ppm, &reply);
    return true;
}

static void pe_handle_data_message(UcsiPpm* ppm, const UcsiPpmPhyPdMsg* msg) {
    const uint8_t type = (uint8_t)(msg->header & PD_HDR_MSG_TYPE_MASK);

    // Source_Capabilities arriving at sink-side WaitForCapabilities, or at
    // SnkReady — a Source may re-advertise at any time to renegotiate, and PD
    // R3.0 §8.3.3.3 routes both through PE_SNK_Evaluate_Capability. Refusing
    // the second case is what made a partner Soft_Reset us mid-contract.
    if((ppm->pe_state == (int)UcsiPpmPeSnkWaitForCapabilities ||
        ppm->pe_state == (int)UcsiPpmPeSnkReady) &&
       type == PD_MSG_TYPE_SOURCE_CAPS_DATA) {
        ppm->pe_received_pdo_count = (uint8_t)(msg->object_count <= UCSI_PPM_MAX_PDOS ? msg->object_count : UCSI_PPM_MAX_PDOS);
        for(uint8_t i = 0; i < ppm->pe_received_pdo_count; ++i) {
            ppm->pe_received_pdos[i] = msg->objects[i];
            const uint32_t p = msg->objects[i];
            if(pdo_is_fixed(p)) {
                UCSI_LOG_I(
                    ppm,
                    "src pdo%u: %u mV %u mA",
                    (unsigned)(i + 1u),
                    (unsigned)pdo_fixed_voltage_mv(p),
                    (unsigned)pdo_fixed_current_ma(p));
            } else {
                UCSI_LOG_I(ppm, "src pdo%u: non-fixed (%08lX)", (unsigned)(i + 1u), (unsigned long)p);
            }
        }
        pe_send_request(ppm);
        return;
    }

    // Request arriving at source-side SrcSendCapabilities.
    if(ppm->pe_state == (int)UcsiPpmPeSrcSendCapabilities && type == PD_MSG_TYPE_REQUEST_DATA) {
        pe_src_handle_request(ppm, msg);
        return;
    }

    if(type == PD_MSG_TYPE_VENDOR_DEFINED && pe_reply_vdm_nak(ppm, msg)) return;

    // Anything else — Alert, Sink_Capabilities and the rest of PD 3.0's
    // catalogue, plus Unstructured VDMs. See pe_reply_not_supported: staying
    // silent is what makes a talkative partner tear the connection down.
    pe_reply_not_supported(ppm, type, false);
}

// Pushes our current roles and revision into SWITCHES1, which is where the
// chip reads them from when it builds auto-GoodCRC headers on its own. Must
// follow every change to either role, or our acknowledgements start describing
// a port we no longer are.
static void pe_sync_phy_header_bits(UcsiPpm* ppm) {
    (void)ucsi_ppm_phy_set_msg_header_bits(ppm, ppm->tc_role_is_src, ppm->pe_data_role_is_dfp);
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

    // Capability queries. Both are mandatory for the role that owns the
    // answer (PD R3.0 §6.4.1.3) and neither depends on PE state: a partner may
    // ask at any point once PD comms are up. Only if we have nothing
    // configured to say does this fall through to Not_Supported.
    if(type == PD_MSG_TYPE_GET_SINK_CAP && ppm->config.sink_caps.count) {
        (void)pe_send_sink_caps(ppm);
        return;
    }
    if(type == PD_MSG_TYPE_GET_SOURCE_CAP && ppm->config.source_caps.count) {
        (void)pe_src_send_caps(ppm);
        return;
    }
    // PD R3.2 §6.5.2: answered by a Source or a DRP, so our being the Sink right
    // now is no reason to refuse. Refusing is what had two different partners
    // re-asking every 23 ms for as long as they stayed plugged in.
    if(type == PD_MSG_TYPE_GET_SOURCE_CAP_EXT) {
        if(pe_send_source_caps_extended(ppm)) return;
        // Fall through to the refusal if the send failed — silence would stall
        // the partner's SenderResponseTimer.
    }
    // PD R3.2 §6.5.3: informational, and answered by whichever side was asked.
    if(type == PD_MSG_TYPE_GET_STATUS) {
        if(pe_send_status(ppm)) return;
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
        pe_sync_phy_header_bits(ppm);
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
            pe_set_state(ppm, (int)UcsiPpmPeSnkWaitForPsRdy);
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
            pe_sync_phy_header_bits(ppm);
            ucsi_ppm_notify_connector_change(ppm, UCSI_PPM_CSC_PARTNER_CHANGED);
        }
        // Accept / Reject / Wait / Not_Supported all return to *Ready —
        // only Accept flips the role. Partner can refuse via Reject or
        // Wait (PD §8.3.3.8) or declare DR_Swap unsupported (PD §6.5);
        // either way we just stay as we were.
        if(type == PD_MSG_TYPE_ACCEPT || type == PD_MSG_TYPE_REJECT || type == PD_MSG_TYPE_WAIT || type == PD_MSG_TYPE_NOT_SUPPORTED) {
            pe_set_state(ppm, ppm->tc_role_is_src ? (int)UcsiPpmPeSrcReady : (int)UcsiPpmPeSnkReady);
        }
        break;
    case(int)UcsiPpmPePrSwapSnkSendSwap:
        if(type == PD_MSG_TYPE_ACCEPT) {
            // Partner committed to the swap. Now wait for partner's PS_RDY
            // signalling its VBUS has dropped to vSafe0V.
            pe_set_state(ppm, (int)UcsiPpmPePrSwapSnkWaitForSourceOff);
            pe_arm_timer(ppm);
        } else if(
            type == PD_MSG_TYPE_REJECT || type == PD_MSG_TYPE_WAIT ||
            type == PD_MSG_TYPE_NOT_SUPPORTED) {
            // Partner refused — contract stays intact, we go back to SnkReady.
            pe_set_state(ppm, (int)UcsiPpmPeSnkReady);
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
            // Written straight rather than through tc_set_state, which is
            // private to the TC layer — log it here so the trace still shows
            // every Type-C state change.
            UCSI_LOG_I(ppm, "Attached.SNK -> Attached.SRC (pr_swap)");
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
            pe_set_state(ppm, (int)UcsiPpmPePrSwapSnkSourceOn);
            pe_arm_timer(ppm);
        }
        break;
    default:
        break;
    }

    // Nothing above claimed it. Every branch that did either returned or
    // dealt with a response type, so anything reaching here is a request we
    // do not implement and owes the partner an answer.
    if(pe_control_needs_reply(type)) {
        pe_reply_not_supported(ppm, type, false);
    }
}

// --- public API ------------------------------------------------------------

UcsiPpmStatus ucsi_ppm_pe_init(UcsiPpm* ppm) {
    pe_set_state(ppm, (int)UcsiPpmPeStateIdle);
    ppm->pe_timer_start_ms = 0u;
    memset(ppm->pe_received_pdos, 0, sizeof(ppm->pe_received_pdos));
    ppm->pe_received_pdo_count = 0u;
    ppm->pe_requested_pdo_index = 0u;
    ppm->pe_negotiated_voltage_mv = 0u;
    ppm->pe_negotiated_current_ma = 0u;
    ppm->pe_caps_counter = 0u;
    ppm->pe_current_rdo = 0u;
    ppm->pe_hard_reset_counter = 0u;
    ppm->pe_typec_only = false;
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
    pe_set_state(ppm, (int)UcsiPpmPeSnkWaitForCapabilities);
    pe_arm_timer(ppm);
}

void ucsi_ppm_pe_on_attach_src(UcsiPpm* ppm) {
    (void)ucsi_ppm_pe_init(ppm);
    ppm->pe_data_role_is_dfp = true;
    pe_set_state(ppm, (int)UcsiPpmPeSrcSendCapabilities);
    ppm->pe_caps_counter = 0u;
    pe_arm_timer(ppm); // SourceCapabilityTimer

    // Advertise immediately only when the supply is already up. TC switched
    // VBUS on a millisecond ago and an external OTG regulator needs tens of
    // milliseconds to reach vSafe5V — PD R3.0 §8.3.3.2 has the source wait
    // for its power supply, and a partner that hears Source_Capabilities with
    // no VBUS behind them is entitled to ignore the whole exchange. When it
    // is not ready yet the SourceCapabilityTimer sends the first one, still
    // well inside tFirstSourceCap.
    bool vbus_ok = false;
    if(ucsi_ppm_phy_read_vbusok(ppm, &vbus_ok) != UcsiPpmStatusOk || !vbus_ok) {
        UCSI_LOG_I(ppm, "vbus not up yet, first source caps deferred");
        return;
    }
    if(!pe_src_send_caps(ppm)) {
        pe_to_error(ppm);
        return;
    }
    ppm->pe_caps_counter = 1u;
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
    pe_set_state(ppm, (int)UcsiPpmPeSrcReady);
    ppm->pe_hard_reset_counter = 0u;
    ucsi_ppm_notify_connector_change(ppm, UCSI_PPM_CSC_POWER_OP_MODE_CHANGE | UCSI_PPM_CSC_NEGOTIATED_PL_CHANGE);
}

void ucsi_ppm_pe_handle_message(UcsiPpm* ppm, const UcsiPpmPhyPdMsg* msg) {
    if(!msg || msg->sop_type != UcsiPpmPhySopTypeSop) return;

    // The partner just spoke PD, so any earlier conclusion that it could not
    // is void — go back to running the protocol timers.
    if(ppm->pe_typec_only) {
        UCSI_LOG_I(ppm, "partner started talking PD after all");
        ppm->pe_typec_only = false;
        ppm->pe_hard_reset_counter = 0u;
    }

    if(ppm->pe_state == (int)UcsiPpmPeStateIdle || ppm->pe_state == (int)UcsiPpmPeStateError) {
        return;
    }

    // PD R3.0 §6.5: an Extended Message sets bit 15 and its Message Type comes
    // from a namespace of its own. Falling through to the Data path would read
    // the number as a Data type — extended 0x01 is Source_Capabilities_Extended
    // and would be taken for Source_Capabilities, whose "PDOs" would then be a
    // slice of somebody's serial number. We implement none of them, so refuse
    // before the type is ever interpreted.
    if(msg->header & PD_HDR_EXTENDED_BIT) {
        pe_reply_not_supported(ppm, (uint8_t)(msg->header & PD_HDR_MSG_TYPE_MASK), true);
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
        pe_set_state(ppm, (int)UcsiPpmPeSrcSendCapabilities);
        ppm->pe_caps_counter = 0u;
        if(!pe_src_send_caps(ppm)) {
            pe_to_error(ppm);
            return;
        }
        ppm->pe_caps_counter = 1u;
        pe_arm_timer(ppm);
    } else {
        pe_set_state(ppm, (int)UcsiPpmPeSnkWaitForCapabilities);
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
    pe_set_state(ppm, (int)UcsiPpmPeSnkWaitForAccept);
    pe_arm_timer(ppm);
    // We start this exchange rather than answer one, so there is no receive in
    // progress to wait for: the message goes out before we return, and the
    // SenderResponseTimer just armed measures from the wire, not from a queue.
    ucsi_ppm_prl_flush_tx(ppm);
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
    pe_set_state(ppm, (int)UcsiPpmPeWaitForDrSwapResponse);
    pe_arm_timer(ppm);
    ucsi_ppm_prl_flush_tx(ppm);
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
    pe_set_state(ppm, (int)UcsiPpmPePrSwapSnkSendSwap);
    pe_arm_timer(ppm);
    ucsi_ppm_prl_flush_tx(ppm);
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
        case(int)UcsiPpmPeSrcTransitionSupply:
        case(int)UcsiPpmPeWaitForDrSwapResponse:
            pe_request_soft_reset(ppm);
            break;
        case(int)UcsiPpmPeSrcSendCapabilities:
            // PD R3.0 §8.3.3.2.3: no GoodCRC for Source_Capabilities is a
            // Protocol Error that sends PE_SRC_Send_Capabilities back to
            // PE_SRC_Discovery — retry the advertisement, do not reset. A
            // partner that never answers has no PD state to resynchronise, so
            // Soft_Reset draws another retry-fail and the escalation runs away
            // to Hard Reset and Error against a device that is merely not PD.
            // The nCapsCount retry in ucsi_ppm_pe_tick owns the outcome.
            pe_arm_timer(ppm);
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
        // Nothing left to wait for once the partner is known to be Type-C
        // only; we stay here passively in case it ever starts talking PD.
        if(ppm->pe_typec_only) break;
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
            // tc_role_is_src already flipped when the partner's PS_RDY arrived.
            pe_sync_phy_header_bits(ppm);
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
            pe_set_state(ppm, (int)UcsiPpmPeSrcSendCapabilities);
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
        if(ppm->pe_typec_only) break;
        if(pe_timer_expired(ppm, UCSI_PPM_PE_SOURCE_CAP_MS)) {
            // Advertising without a supply behind it is worse than staying
            // quiet — the partner may write us off as broken. Wait it out
            // without spending a retry.
            bool vbus_ok = false;
            if(ucsi_ppm_phy_read_vbusok(ppm, &vbus_ok) != UcsiPpmStatusOk || !vbus_ok) {
                UCSI_LOG_I(ppm, "vbus not up yet, holding off source caps");
                pe_arm_timer(ppm);
                break;
            }
            if(ppm->pe_caps_counter >= UCSI_PPM_PE_CAPS_COUNT_MAX) {
                // PD R3.0 §8.3.3.2.4: nCapsCount advertisements with no
                // Request means the partner does not do PD, which is
                // PE_SRC_Disabled — VBUS stays up and Rp keeps advertising
                // Type-C current. Mirrors the sink's conclusion in
                // pe_request_hard_reset; Error would disown a working
                // connection to something as ordinary as a flash drive.
                UCSI_LOG_I(ppm, "partner is not PD capable, staying a type-c only source");
                ppm->pe_typec_only = true;
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

// Mirrors the ucsi_ppm_pe_tick switch: every state that checks a timer there
// reports the remaining time here. Keep the two in sync when adding states.
uint32_t ucsi_ppm_pe_next_timeout_ms(const UcsiPpm* ppm) {
    switch(ppm->pe_state) {
    case(int)UcsiPpmPeSnkWaitForCapabilities:
        if(ppm->pe_typec_only) return UCSI_PPM_NO_TIMEOUT;
        return pe_timer_remaining_ms(ppm, UCSI_PPM_PE_SINK_WAIT_CAP_MS);
    case(int)UcsiPpmPeSnkWaitForAccept:
    case(int)UcsiPpmPeWaitForSoftResetAccept:
    case(int)UcsiPpmPeWaitForDrSwapResponse:
    case(int)UcsiPpmPePrSwapSnkSendSwap:
        return pe_timer_remaining_ms(ppm, UCSI_PPM_PE_SENDER_RESPONSE_MS);
    case(int)UcsiPpmPePrSwapSnkWaitForSourceOff:
        return pe_timer_remaining_ms(ppm, UCSI_PPM_PE_PS_SOURCE_OFF_MS);
    case(int)UcsiPpmPePrSwapSnkSourceOn: {
        // tick polls VBUSOK over I2C in this state, so request short wakeups:
        // first until the settle window closes, then every SOURCE_ON_POLL
        // interval, never sleeping past the PSSourceOnTimer give-up.
        const uint32_t settle = pe_timer_remaining_ms(ppm, UCSI_PPM_PE_PR_SWAP_SRC_SETTLE_MS);
        const uint32_t give_up = pe_timer_remaining_ms(ppm, UCSI_PPM_PE_PS_SOURCE_ON_MS);
        const uint32_t next = settle ? settle : UCSI_PPM_PE_SOURCE_ON_POLL_MS;
        return next < give_up ? next : give_up;
    }
    case(int)UcsiPpmPeSnkWaitForPsRdy:
    case(int)UcsiPpmPeSrcTransitionSupply:
        return pe_timer_remaining_ms(ppm, UCSI_PPM_PE_PS_TRANSITION_MS);
    case(int)UcsiPpmPeSrcSendCapabilities:
        if(ppm->pe_typec_only) return UCSI_PPM_NO_TIMEOUT;
        return pe_timer_remaining_ms(ppm, UCSI_PPM_PE_SOURCE_CAP_MS);
    default:
        return UCSI_PPM_NO_TIMEOUT;
    }
}
