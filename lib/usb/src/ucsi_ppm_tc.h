#pragma once

// L3 Type-C state machine — connector lifecycle.
// Plan: lib/usb/plan/type-c-sm.md. This file is internal to ucsi_ppm.
//
// Owns the Unattached ↔ AttachWait ↔ Attached transitions. Drives the
// FUSB302 auto-TOGGLE feature for partner detection, picks CC orientation
// from TOGSS, and routes BMC TX/RX to the active CC pin. Does not know
// about PD messages — that's PRL/PE.

#include "ucsi_ppm.h"
#include "ucsi_ppm_i.h"
#include "ucsi_ppm_phy.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UcsiPpmTcStateDisabled, // terminations off; no toggle running
    UcsiPpmTcStateUnattached, // toggling, waiting for ToggleDone
    UcsiPpmTcStateAttachWait, // TOGSS settled, polarity locked, debounce pending (1b)
    UcsiPpmTcStateAttachedSrc, // partner attached, we are source
    UcsiPpmTcStateAttachedSnk, // partner attached, we are sink
    UcsiPpmTcStateErrorRecovery, // transient; force unattached cycle
} UcsiPpmTcState;

// Enters initial state per config.initial_cc_operation_mode:
//   Disabled                  → Disabled (no toggle, no terminations)
//   RpOnly/RdOnly/Drp         → Unattached + phy_start_toggle(mode)
// Returns the result of the underlying phy operations; any HAL error
// propagates upward to ucsi_ppm_init.
UcsiPpmStatus ucsi_ppm_tc_init(UcsiPpm* ppm);

// Stops toggling; leaves the chip in deinit's "inert" state. Best-effort.
UcsiPpmStatus ucsi_ppm_tc_deinit(UcsiPpm* ppm);

// Resets TC state and re-enters initial state (same as init). Used by
// ucsi_ppm_reset and as part of detach recovery.
UcsiPpmStatus ucsi_ppm_tc_reset(UcsiPpm* ppm);

// Consumes one event from the PHY pump. Handles only events relevant to
// the connector state machine (ToggleDone, VbusChanged today; CompChanged /
// HardReset* once detach/recovery is in). Unknown events are dropped
// silently — PRL/PE will pick them up via their own sinks later.
void ucsi_ppm_tc_handle_phy_event(UcsiPpm* ppm, const UcsiPpmPhyEvent* event);

// Time-tick — re-evaluates time-dependent transitions (AttachWait debounce
// expiry today). Cheap no-op when nothing has changed. Call from
// `ucsi_ppm_tick` after the PHY pump.
void ucsi_ppm_tc_tick(UcsiPpm* ppm);

// Recomputes how much current we may draw as a sink — from the PD contract
// if there is one, otherwise from the source's Type-C Rp advertisement — and
// reports it through config.sink_current_limit when it changed. Cheap: the
// Rp reading is cached, so this touches no I2C. Called at the end of every
// ucsi_ppm_tick, which is what makes it pick up TC and PE changes alike.
void ucsi_ppm_tc_update_sink_current_limit(UcsiPpm* ppm);

// True when we may open an Atomic Message Sequence of our own. PD 3.0
// collision avoidance (R3.2 §7.2): a Sink reads the Source's Rp off BC_LVL and
// Shall Not start an AMS while it reads SinkTxNG. Always true for a Source (it
// is the one signalling), when not Attached.SNK, for a partner we speak R2.0
// to, and outside an Explicit Contract, where Rp still means Type-C Current.
// Only sequences we *start* are gated — answers inside the partner's own AMS,
// Soft_Reset and Hard Reset are not.
bool ucsi_ppm_tc_sink_tx_allowed(const UcsiPpm* ppm);

// The Source's half of collision avoidance (R3.2 Figure 9.8): keeps
// CONTROL0.HOST_CUR at SinkTxOk (3.0 A) while we sit idle in PE_SRC_Ready with
// an Explicit Contract, at SinkTxNG (1.5 A) while an AMS of ours is pending, and
// at config.source_rp_current otherwise — before a contract Rp is still a
// Type-C Current advertisement and must stay honest. Level-triggered and
// change-gated: costs no I2C when the answer has not moved, so it is safe to
// call from the end of every ucsi_ppm_tick.
void ucsi_ppm_tc_update_source_rp(UcsiPpm* ppm);

// Same policy, but writes unconditionally. For the paths that have just
// re-initialised the chip, where CONTROL0 is back at its reset value and the
// cache ucsi_ppm_tc_update_source_rp keeps would skip the write.
void ucsi_ppm_tc_apply_source_rp(UcsiPpm* ppm);

// Milliseconds until the next TC deadline (CCDebounce expiry or the
// AttachWait give-up timeout), or UCSI_PPM_NO_TIMEOUT when the current
// state has no timed transition. Backend for ucsi_ppm_next_timeout_ms.
uint32_t ucsi_ppm_tc_next_timeout_ms(const UcsiPpm* ppm);

#ifdef __cplusplus
}
#endif
