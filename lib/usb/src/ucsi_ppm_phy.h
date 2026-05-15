#pragma once

// L4 PHY driver — minimal FUSB302 wrapper for the ucsi_ppm library.
// Architecture: plan/architecture.md §7. Implementation notes: plan/fusb302.md.
//
// This driver intentionally has *no* PD or Type-C state-machine logic — it
// only knows FUSB302 register layout and translates raw INT flags into
// typed events for L3 (Type-C SM / PRL / PE).
//
// All I/O goes through the I²C callbacks in UcsiPpmConfig; the driver does
// not depend on Furi/Pico HAL directly.

#include "ucsi_ppm.h"
#include "ucsi_ppm_i.h"

#ifdef __cplusplus
extern "C" {
#endif

// --- Types -----------------------------------------------------------------

// CONTROL2.MODE values (datasheet §6.x; fusb302_reg.h Fusb302Control2RegBits).
typedef enum {
    UcsiPpmPhyToggleModeSrc, // SRC polling — Rp-only
    UcsiPpmPhyToggleModeSnk, // SNK polling — Rd-only
    UcsiPpmPhyToggleModeDrp, // DRP polling — alternating Rp/Rd
} UcsiPpmPhyToggleMode;

// Active CC pin selection for TX/RX routing.
typedef enum {
    UcsiPpmPhyCcNone, // both off (no orientation locked)
    UcsiPpmPhyCc1,
    UcsiPpmPhyCc2,
} UcsiPpmPhyCc;

// Raw TOGSS field (STATUS1A bits 5:3). Interpretation belongs in L3; we just
// surface the bits. Values per datasheet:
//   0b000 — toggle still running (shouldn't appear with I_TOGDONE asserted)
//   0b001 — settled to SRC on CC1
//   0b010 — settled to SRC on CC2
//   0b101 — settled to SNK on CC1
//   0b110 — settled to SNK on CC2
//   0b111 — Audio Accessory (Ra on both CC; SRC1 state)
typedef uint8_t UcsiPpmPhyTogss;

// Events emitted by the pump. Several events carry extra data the pump
// pre-reads to spare L3 a second I²C round-trip.
typedef enum {
    UcsiPpmPhyEventToggleDone,
    UcsiPpmPhyEventVbusChanged,
    UcsiPpmPhyEventBcLvlChanged,
    UcsiPpmPhyEventCompChanged,
    UcsiPpmPhyEventCollision,
    UcsiPpmPhyEventMessageRx, // RX FIFO has a message (or several)
    UcsiPpmPhyEventTxSuccess, // I_TXSENT — GoodCRC received
    UcsiPpmPhyEventTxRetryFail, // I_RETRYFAIL — retries exhausted
    UcsiPpmPhyEventHardResetRx, // I_HARDRST
    UcsiPpmPhyEventHardResetSent, // I_HARDSENT
} UcsiPpmPhyEventKind;

typedef struct {
    UcsiPpmPhyEventKind kind;
    union {
        UcsiPpmPhyTogss togss; // ToggleDone
        bool vbus_ok; // VbusChanged
        uint8_t bc_lvl; // BcLvlChanged (2-bit STATUS0.BC_LVL)
        bool comp_above; // CompChanged (STATUS0.COMP)
    } u;
} UcsiPpmPhyEvent;

typedef void (*UcsiPpmPhyEventSink)(void* ctx, const UcsiPpmPhyEvent* event);

// --- Lifecycle -------------------------------------------------------------

// Brings up FUSB302: SW_RESET, POWER=0x0F (all blocks on), all relevant
// interrupt masks open, global INT_MASK = 0. After init the chip is silent
// — no TOGGLE running, no PD, no terminations. L3 starts the desired
// activity via the calls below.
UcsiPpmStatus ucsi_ppm_phy_init(UcsiPpm* ppm);

// Best-effort teardown: terminations Open (Rd/Rp/VCONN all off), AUTO_CRC
// off, TOGGLE off. Leaves chip powered but inert.
UcsiPpmStatus ucsi_ppm_phy_deinit(UcsiPpm* ppm);

// Full software reset (RESET.SW_RESET = 1). Resets all registers to default.
// Used internally by init and by ucsi_ppm_reset.
UcsiPpmStatus ucsi_ppm_phy_sw_reset(UcsiPpm* ppm);

// PD-only reset (RESET.PD_RESET = 1): clears FIFOs, MessageID counters,
// retry counter. Used after Hard Reset / Soft Reset on the PE side.
UcsiPpmStatus ucsi_ppm_phy_pd_reset(UcsiPpm* ppm);

// --- Type-C primitives -----------------------------------------------------

// Starts FUSB302 auto-toggling in the selected mode. Emits TOGGLE_DONE when
// the chip detects a partner.
UcsiPpmStatus ucsi_ppm_phy_start_toggle(UcsiPpm* ppm, UcsiPpmPhyToggleMode mode);

// Stops auto-toggle (CONTROL2.TOGGLE = 0).
UcsiPpmStatus ucsi_ppm_phy_stop_toggle(UcsiPpm* ppm);

// Reads STATUS1A.TOGSS. Caller must consult this after a TOGGLE_DONE event
// (or read it explicitly when needed).
UcsiPpmStatus ucsi_ppm_phy_read_toggle_result(UcsiPpm* ppm, UcsiPpmPhyTogss* out);

// Locks polarity on the given CC pin (MEAS_CC* in SWITCHES0, TX_CC* in
// SWITCHES1). Pass UcsiPpmPhyCcNone to clear both.
UcsiPpmStatus ucsi_ppm_phy_lock_polarity(UcsiPpm* ppm, UcsiPpmPhyCc cc);

// Sets advertised Rp current (CONTROL0.HOST_CUR). Only meaningful when
// SWITCHES0.PU_EN* is enabled.
UcsiPpmStatus ucsi_ppm_phy_set_rp_current(UcsiPpm* ppm, UcsiPpmRpCurrent current);

// Sets the header bits FUSB302 uses to build auto-GoodCRC responses:
// SWITCHES1.{POWER_ROLE, DATA_ROLE, SPEC_REV}. Must reflect the current
// negotiated PD state.
UcsiPpmStatus ucsi_ppm_phy_set_msg_header_bits(
    UcsiPpm* ppm,
    bool power_role_src,
    bool data_role_dfp,
    uint8_t spec_rev);

// --- PD path ---------------------------------------------------------------

// Enables PD reception: SWITCHES1.AUTO_CRC = 1, CONTROL3.{AUTO_RETRY,
// N_RETRIES}. Pass n_retries in 0..3 (datasheet field is "retries beyond
// the initial attempt"; 2 is the PD spec value nRetryCount).
UcsiPpmStatus ucsi_ppm_phy_enable_pd(UcsiPpm* ppm, uint8_t n_retries);

// Disables AUTO_CRC. Used when partner detaches.
UcsiPpmStatus ucsi_ppm_phy_disable_pd(UcsiPpm* ppm);

// Triggers Hard Reset BMC pattern (CONTROL3.SEND_HARD_RESET = 1).
// Self-clearing in hardware; emits HARD_RESET_SENT event on completion.
UcsiPpmStatus ucsi_ppm_phy_send_hard_reset(UcsiPpm* ppm);

// FIFO management. Both are non-blocking writes of the corresponding flush
// bit; FUSB302 self-clears them. plan/fusb302.md §8.2 — fix #1.
UcsiPpmStatus ucsi_ppm_phy_flush_tx(UcsiPpm* ppm);
UcsiPpmStatus ucsi_ppm_phy_flush_rx(UcsiPpm* ppm);

// --- Interrupt pump --------------------------------------------------------

// Reads INTERRUPT, INTERRUPTA, INTERRUPTB (which clears them in HW) and
// emits a typed event for each asserted bit via `sink`. Bits whose
// follow-up state is needed (TOGSS, VBUSOK, BC_LVL, COMP) are read and
// included in the event. Call from `ucsi_ppm_tick` or right after
// `ucsi_ppm_notify_fusb302_irq` raised its flag.
//
// The sink callback is invoked synchronously from this function; it must
// not call back into ucsi_ppm public API (api.md §8 reentrancy rules).
UcsiPpmStatus ucsi_ppm_phy_pump(UcsiPpm* ppm, UcsiPpmPhyEventSink sink, void* sink_ctx);

#ifdef __cplusplus
}
#endif
