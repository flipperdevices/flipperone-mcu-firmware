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
// the connector state machine (ToggleDone today; VbusChanged / CompChanged /
// HardReset* once Attached states are implemented). Unknown events are
// dropped silently — PRL/PE will pick them up via their own sinks later.
void ucsi_ppm_tc_handle_phy_event(UcsiPpm* ppm, const UcsiPpmPhyEvent* event);

#ifdef __cplusplus
}
#endif
