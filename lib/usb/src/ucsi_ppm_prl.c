#include "ucsi_ppm_prl.h"

#include "ucsi_ppm_pe.h"

// PD R3.0 Message Header — MessageID field, bits 11:9.
#define MSG_HDR_MSG_ID_SHIFT 9u
#define MSG_HDR_MSG_ID_MASK  ((uint16_t)(0x07u << MSG_HDR_MSG_ID_SHIFT))

// MessageIDCounter wraps after 0..7 (PD R3.0 §6.8.1 nMessageIDCount = 7).
#define PRL_MSG_ID_WRAP 8u

UcsiPpmStatus ucsi_ppm_prl_init(UcsiPpm* ppm) {
    ppm->prl_next_tx_msg_id = 0u;
    ppm->prl_last_rx_msg_id = 0u;
    ppm->prl_last_rx_valid = false;
    ppm->prl_messages_delivered = 0u;
    return UcsiPpmStatusOk;
}

UcsiPpmStatus ucsi_ppm_prl_reset(UcsiPpm* ppm) {
    return ucsi_ppm_prl_init(ppm);
}

UcsiPpmStatus ucsi_ppm_prl_send_message(UcsiPpm* ppm, UcsiPpmPhyPdMsg* msg) {
    if(!msg) return UcsiPpmStatusInvalidArg;

    // Stamp the next MessageID into the header. Other header fields
    // (msg type, NDO etc) are left alone — PE owns them.
    msg->header = (uint16_t)(
        (msg->header & ~MSG_HDR_MSG_ID_MASK) |
        ((uint16_t)(ppm->prl_next_tx_msg_id & 0x07u) << MSG_HDR_MSG_ID_SHIFT));

    const UcsiPpmStatus s = ucsi_ppm_phy_send_message(ppm, msg);
    if(s != UcsiPpmStatusOk) return s;

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
        if(s != UcsiPpmStatusOk || !received) break;

        // SOP duplicate detection (PD R3.0 §6.8.1.1). SOP'/SOP'' aren't in
        // scope for v1 — pass them through without dedup (the chip won't
        // ack them either since ENSOP* is 0 in init, but be safe).
        if(msg.sop_type == UcsiPpmPhySopTypeSop) {
            const uint8_t rx_id =
                (uint8_t)((msg.header >> MSG_HDR_MSG_ID_SHIFT) & 0x07u);
            if(ppm->prl_last_rx_valid && ppm->prl_last_rx_msg_id == rx_id) {
                // Hardware already sent GoodCRC; we just discard the frame.
                continue;
            }
            ppm->prl_last_rx_msg_id = rx_id;
            ppm->prl_last_rx_valid = true;
        }

        // Deliver to PE for state-machine processing.
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
        // PD R3.0 §6.8.2: Hard Reset clears MessageIDCounter on both ends.
        // PE coordination (state machine effects) lives elsewhere.
        (void)ucsi_ppm_prl_reset(ppm);
        break;
    case UcsiPpmPhyEventTxSuccess:
    case UcsiPpmPhyEventTxRetryFail:
    case UcsiPpmPhyEventCollision:
        // TX outcomes are PE's concern (continue / Soft Reset / Hard Reset).
        // TODO: notify PE.
        break;
    default:
        // ToggleDone / VBUS / BC_LVL / Comp — not for PRL.
        break;
    }
}
