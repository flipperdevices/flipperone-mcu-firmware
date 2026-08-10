#include <furi.h>
#include "ucsi_ppm_prl.h"
#include "ucsi_ppm_pe.h"

#define TAG "UcsiPrl"

// PD R3.0 Message Header — MessageID field, bits 11:9.
#define MSG_HDR_MSG_ID_SHIFT 9u
#define MSG_HDR_MSG_ID_MASK  ((uint16_t)(0x07u << MSG_HDR_MSG_ID_SHIFT))

// PD R3.0 Message Header — Message Type (bits 4:0) and Number of Data Objects
// (bits 14:12). A Soft_Reset is a Control message: type=0x0D, NDO=0.
#define MSG_HDR_MSG_TYPE_MASK  0x001Fu
#define MSG_HDR_NUM_OBJ_SHIFT  12u
#define MSG_HDR_NUM_OBJ_MASK   ((uint16_t)(0x07u << MSG_HDR_NUM_OBJ_SHIFT))
#define PD_MSG_TYPE_GOODCRC    0x01u
#define PD_MSG_TYPE_SOFT_RESET 0x0Du

// MessageIDCounter wraps after 0..7 (PD R3.0 §6.8.1 nMessageIDCount = 7).
#define PRL_MSG_ID_WRAP 8u

// PD R3.0 Message Header — Specification Revision, bits 7:6.
#define MSG_HDR_SPEC_REV_SHIFT 6u
#define MSG_HDR_SPEC_REV_MASK  ((uint16_t)(0x03u << MSG_HDR_SPEC_REV_SHIFT))

void ucsi_ppm_prl_reset_spec_rev(UcsiPpm* ppm) {
    // PD R3.0 §6.1.3.1: start out claiming R3.0 and step down once a partner
    // reveals it is older. Starting low instead would strand us on R2.0 with
    // partners that never re-advertise.
    ppm->prl_our_spec_rev = UCSI_PPM_SPEC_REV_3_0;
}

UcsiPpmStatus ucsi_ppm_prl_init(UcsiPpm* ppm) {
    (void)ucsi_ppm_prl_reset(ppm);
    ucsi_ppm_prl_reset_spec_rev(ppm);
    return UcsiPpmStatusOk;
}

// Applies PD R3.0 §6.2.1.1.5 to a received SOP header: we may never operate
// above the partner's revision, so our outgoing revision is the lower of the
// two. It only ratchets down — a partner that starts speaking R3.0 later in
// the same connection does not lift us back up, because the messages we
// already sent committed us. Detach and Hard Reset clear the ratchet.
static void prl_observe_spec_rev(UcsiPpm* ppm, uint16_t header) {
    uint8_t peer = (uint8_t)((header & MSG_HDR_SPEC_REV_MASK) >> MSG_HDR_SPEC_REV_SHIFT);
    // 00b is the deprecated R1.0 encoding; treat it as R2.0 rather than as a
    // revision below everything we know how to speak.
    if(peer == 0u) peer = UCSI_PPM_SPEC_REV_2_0;
    if(peer >= ppm->prl_our_spec_rev) return;

    UCSI_LOG_I(
        ppm,
        "spec rev: partner is %s, dropping ours to match",
        peer == UCSI_PPM_SPEC_REV_2_0 ? "PD 2.0" : "older");
    ppm->prl_our_spec_rev = peer;
    // The chip builds auto-GoodCRC headers itself out of SWITCHES1, so the
    // revision has to land there too. Leaving it stale would have us answer a
    // partner's frame with one revision and follow it with another.
    (void)ucsi_ppm_phy_set_msg_header_bits(
        ppm, ppm->tc_role_is_src, ppm->pe_data_role_is_dfp, peer);
}

// Soft_Reset and Hard Reset both land here. Neither renegotiates the PD
// revision: they resynchronise MessageID counters with the same partner, and
// that partner is no newer than it was. Only a new attach may raise us back to
// R3.0 — see ucsi_ppm_prl_reset_spec_rev, called from the detach path.
UcsiPpmStatus ucsi_ppm_prl_reset(UcsiPpm* ppm) {
    ppm->prl_next_tx_msg_id = 0u;
    ppm->prl_last_rx_msg_id = 0u;
    ppm->prl_last_rx_valid = false;
    ppm->prl_messages_delivered = 0u;
    return UcsiPpmStatusOk;
}

UcsiPpmStatus ucsi_ppm_prl_send_message(UcsiPpm* ppm, UcsiPpmPhyPdMsg* msg) {
    if(!msg) return UcsiPpmStatusInvalidArg;

    // Stamp the next MessageID into the header. Other header fields
    // (msg type, NDO etc) are left alone — PE owns them.
    msg->header = (uint16_t)((msg->header & ~MSG_HDR_MSG_ID_MASK) | ((uint16_t)(ppm->prl_next_tx_msg_id & 0x07u) << MSG_HDR_MSG_ID_SHIFT));

    UCSI_LOG_I(
        ppm,
        "tx type=0x%02X ndo=%u id=%u", (unsigned)(msg->header & 0x1Fu), (unsigned)((msg->header >> 12) & 0x07u), (unsigned)(ppm->prl_next_tx_msg_id & 0x07u));
    const UcsiPpmStatus s = ucsi_ppm_phy_send_message(ppm, msg);
    if(s != UcsiPpmStatusOk) {
        UCSI_LOG_W(ppm, "tx phy_send failed: %d", (int)s);
        return s;
    }

    // Advance only on successful enqueue. PD R3.0 §6.8.1 says the counter
    // increments after each message sent (regardless of GoodCRC outcome —
    // the retried frame keeps the same MessageID per the spec, but FUSB302
    // handles retry in hardware transparently).
    ppm->prl_next_tx_msg_id = (uint8_t)((ppm->prl_next_tx_msg_id + 1u) % PRL_MSG_ID_WRAP);
    return UcsiPpmStatusOk;
}

// Drains every pending PD message from the RX FIFO, applies SOP duplicate
// detection, and (eventually) delivers each non-dup to PE.
static void prl_drain_rx_fifo(UcsiPpm* ppm) {
    while(1) {
        UcsiPpmPhyPdMsg msg = {0};
        bool received = false;
        const UcsiPpmStatus s = ucsi_ppm_phy_recv_message(ppm, &msg, &received);
        if(s != UcsiPpmStatusOk) {
            // The PHY resynchronizes the FIFO itself where it can; say so
            // either way, because a silent failure here looks exactly like
            // "nothing arrived" and hides a stalled receiver.
            UCSI_LOG_W(ppm, "rx failed: %d", (int)s);
            break;
        }
        if(!received) break;

        // Soft_Reset frames reset PRL state before dedup (PD R3.0 §6.8.1.2).
        // The partner's MessageIDCounter has just rolled back to 0, so our
        // stored last_rx_msg_id would otherwise dedup the legitimate frame.
        const uint8_t msg_type = (uint8_t)(msg.header & MSG_HDR_MSG_TYPE_MASK);
        const uint8_t num_obj = (uint8_t)((msg.header & MSG_HDR_NUM_OBJ_MASK) >> MSG_HDR_NUM_OBJ_SHIFT);

        // GoodCRC belongs to the protocol layer and never reaches PE (PD R3.0
        // §6.3.1). The chip answers incoming frames itself via AUTO_CRC, so a
        // copy landing in the FIFO is a stray we drop before dedup — its
        // MessageID mirrors the frame being acked and would poison
        // prl_last_rx_msg_id for the next real message.
        //
        // Logged rather than dropped silently: an acknowledged transmission
        // and a stalled receiver look identical in a trace otherwise.
        if(msg_type == PD_MSG_TYPE_GOODCRC && num_obj == 0u) {
            UCSI_LOG_D(ppm, "rx goodcrc id=%u", (unsigned)((msg.header >> MSG_HDR_MSG_ID_SHIFT) & 0x07u));
            continue;
        }

        // Before dedup: a duplicate still carries the partner's revision, and
        // this must run on every SOP frame that reaches us, not only the ones
        // PE ends up seeing.
        if(msg.sop_type == UcsiPpmPhySopTypeSop) prl_observe_spec_rev(ppm, msg.header);

        const bool is_soft_reset = (msg_type == PD_MSG_TYPE_SOFT_RESET && num_obj == 0u);
        if(is_soft_reset && msg.sop_type == UcsiPpmPhySopTypeSop) {
            ppm->prl_next_tx_msg_id = 0u;
            ppm->prl_last_rx_valid = false;
        }

        // SOP duplicate detection (PD R3.0 §6.8.1.1). SOP'/SOP'' aren't in
        // scope for v1 — pass them through without dedup (the chip won't
        // ack them either since ENSOP* is 0 in init, but be safe).
        if(msg.sop_type == UcsiPpmPhySopTypeSop) {
            const uint8_t rx_id = (uint8_t)((msg.header >> MSG_HDR_MSG_ID_SHIFT) & 0x07u);
            if(ppm->prl_last_rx_valid && ppm->prl_last_rx_msg_id == rx_id) {
                // Hardware already sent GoodCRC; we just discard the frame.
                continue;
            }
            ppm->prl_last_rx_msg_id = rx_id;
            ppm->prl_last_rx_valid = true;
        }

        // Deliver to PE for state-machine processing.
        UCSI_LOG_I(
        ppm,
            "rx type=0x%02X ndo=%u id=%u sop=%u", (unsigned)msg_type, (unsigned)num_obj, (unsigned)((msg.header >> MSG_HDR_MSG_ID_SHIFT) & 0x07u), (unsigned)msg.sop_type);
        ucsi_ppm_pe_handle_message(ppm, &msg);
        ppm->prl_messages_delivered++;
    }
}

void ucsi_ppm_prl_handle_phy_event(UcsiPpm* ppm, const UcsiPpmPhyEvent* event) {
    switch(event->kind) {
    case UcsiPpmPhyEventMessageRx:
        prl_drain_rx_fifo(ppm);
        break;
    case UcsiPpmPhyEventHardResetRx:
    case UcsiPpmPhyEventHardResetSent:
        // PD R3.0 §6.8.2: a Hard Reset resets the protocol layer on both ends
        // and discards everything in flight. Clearing our MessageID counters
        // is only half of that — the chip's FIFOs still hold frames from
        // before the reset. Delivering one afterwards makes PE answer a
        // partner that has already torn the session down, which then times
        // out waiting for a reply that can never come.
        //
        // Safe to do here: the pump emits HardResetRx (INTERRUPTA) before
        // MessageRx (INTERRUPTB), so the stale frame is gone before we drain.
        (void)ucsi_ppm_phy_flush_rx(ppm);
        (void)ucsi_ppm_phy_flush_tx(ppm);
        UCSI_LOG_I(ppm, "hard reset: fifos flushed, msg ids cleared");
        // PE coordination (state machine effects) lives elsewhere.
        (void)ucsi_ppm_prl_reset(ppm);
        break;
    case UcsiPpmPhyEventTxSuccess:
    case UcsiPpmPhyEventTxRetryFail:
    case UcsiPpmPhyEventCollision:
        // TX outcomes are PE's concern (continue / Soft Reset / Hard Reset).
        // PE receives the same phy event via ucsi_ppm_pe_handle_phy_event;
        // PRL state itself stays put — PE resets it explicitly when it
        // initiates Soft Reset (so msg_id=0 goes out).
        break;
    default:
        // ToggleDone / VBUS / BC_LVL / Comp — not for PRL.
        break;
    }
}
