#pragma once

// L3 PRL (Protocol Layer) — thin layer between PE and FUSB302.
// Plan: lib/usb/plan/prl-sm.md. FUSB302 handles most of PRL in hardware
// (auto-GoodCRC, auto-retry, CRC32), so the software responsibilities
// reduce to:
//   - MessageID stamping on TX (PD R3.0 §6.2.1.1.4 / §6.8.1).
//   - Duplicate detection on RX (per-SOP MessageIDCounter tracking).
//   - State reset on Hard Reset / Soft Reset / detach.
//   - Routing PD messages between PE and the PHY FIFOs.
//
// v1 only handles SOP. SOP'/SOP'' are reserved for future cable comms.

#include "ucsi_ppm.h"
#include "ucsi_ppm_i.h"
#include "ucsi_ppm_phy.h"

#ifdef __cplusplus
extern "C" {
#endif

// Resets PRL counters to "session start" state. Called by ucsi_ppm_init
// and on every Hard Reset / detach.
UcsiPpmStatus ucsi_ppm_prl_init(UcsiPpm* ppm);
UcsiPpmStatus ucsi_ppm_prl_reset(UcsiPpm* ppm);

/** Puts the queued outgoing message, if any, on the wire. Called once the RX
 * FIFO is drained — see ucsi_ppm_prl_send_message for why answers wait.
 * A frame waiting to be retried after a collision goes out here too, after
 * anything PE queued and only once nothing else is awaiting the chip's
 * verdict. */
void ucsi_ppm_prl_flush_tx(UcsiPpm* ppm);

/** Drops a frame parked for retry after a collision. For PE to call when the
 * message that just arrived made that frame obsolete — a Source that
 * re-advertised its capabilities will get a fresh Request, not the one that
 * collided with the advertisement. Nothing to do when no retry is parked. */
void ucsi_ppm_prl_discard_retry(UcsiPpm* ppm);

/** Restores the advertised PD revision to R3.0. Belongs to detach only: the
 * revision describes the partner, so it outlives Soft_Reset and Hard Reset and
 * dies with the connection. */
void ucsi_ppm_prl_reset_spec_rev(UcsiPpm* ppm);

// Stamps the current MessageID into `msg->header` (overwriting bits 11:9)
// and forwards to ucsi_ppm_phy_send_message. On success the next-MessageID
// counter advances (mod 8). The caller's `msg` is mutated in place.
UcsiPpmStatus ucsi_ppm_prl_send_message(UcsiPpm* ppm, UcsiPpmPhyPdMsg* msg);

// Routes a PHY event to PRL: MessageRx drains the FIFO and dedups; Hard
// Reset events reset PRL counters; TxSuccess / TxRetryFail retire the frame
// in flight; Collision parks it for a retry (or, past PRL_COLLISION_RETRY_MAX,
// hands PE a TxRetryFail). Other event kinds are ignored — they belong to TC
// or other layers.
void ucsi_ppm_prl_handle_phy_event(UcsiPpm* ppm, const UcsiPpmPhyEvent* event);

#ifdef __cplusplus
}
#endif
