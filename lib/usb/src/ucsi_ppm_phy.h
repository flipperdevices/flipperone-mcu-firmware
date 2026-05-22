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

// Configures SWITCHES0 for source role on the given CC: clears the sink
// PDWN bits, asserts PU_EN for the active CC, routes MEAS to the same CC.
// Call once after TOGGLE_DONE picks SRC — the chip's toggle FSM does NOT
// keep PU_EN latched on its own.
UcsiPpmStatus ucsi_ppm_phy_set_source_termination(UcsiPpm* ppm, UcsiPpmPhyCc cc);

// Reads the current STATUS0.VBUSOK level — non-edge polling, useful when
// the partner's VBUS was already up before phy_init ran (no I_VBUSOK edge
// ever fired so the event-driven path misses it).
UcsiPpmStatus ucsi_ppm_phy_read_vbusok(UcsiPpm* ppm, bool* out_vbus_ok);

// Sets the header bits FUSB302 uses to build auto-GoodCRC responses:
// SWITCHES1.{POWER_ROLE, DATA_ROLE, SPEC_REV}. Must reflect the current
// negotiated PD state.
UcsiPpmStatus ucsi_ppm_phy_set_msg_header_bits(UcsiPpm* ppm, bool power_role_src, bool data_role_dfp, uint8_t spec_rev);

// --- PD messages -----------------------------------------------------------

// Maximum number of Data Objects in a PD message (spec: 7).
#define UCSI_PPM_PHY_MAX_OBJECTS 7

// SOP* destination for outgoing / incoming PD messages.
// v1 only originates SOP (port partner); SOP'/SOP'' are reserved for future
// cable communication and are accepted by the encoder for forward compat.
typedef enum {
    UcsiPpmPhySopTypeSop, // SOP — port partner
    UcsiPpmPhySopTypeSopPrime, // SOP' — first cable plug
    UcsiPpmPhySopTypeSopDoublePrime, // SOP'' — second cable plug
} UcsiPpmPhySopType;

typedef struct {
    UcsiPpmPhySopType sop_type;
    uint16_t header;
    uint32_t objects[UCSI_PPM_PHY_MAX_OBJECTS];
    uint8_t object_count; // 0..7; control messages use 0
} UcsiPpmPhyPdMsg;

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

// Encodes the PD message into the FUSB302 TX FIFO and triggers transmission
// via a single I²C burst write to the FIFOS register:
//   SOP destination tokens (4 bytes) +
//   PACKSYM with payload length +
//   PD message header (2 bytes, little-endian) +
//   data objects (4 bytes each, little-endian) +
//   JAMCRC + EOP + TXOFF + TXON
//
// The hardware computes CRC32, encodes BMC, and transmits. Completion
// surfaces later via `pump` as TxSuccess (I_TXSENT) or TxRetryFail
// (I_RETRYFAIL). This call does **not** flush the TX FIFO or wait for any
// earlier transmission — caller orchestrates that via flush_tx and the
// previous send's completion event (plan/fusb302.md §8.2 fix #1).
//
// The "Number of Data Objects" field (header bits 14:12) is overwritten to
// match `object_count` so the spec invariant (PD R3.0 §6.2.1.1.5) holds
// even if the caller's header value is stale.
UcsiPpmStatus ucsi_ppm_phy_send_message(UcsiPpm* ppm, const UcsiPpmPhyPdMsg* msg);

// Reads one PD message from the RX FIFO into `*out`. Drains:
//   1 byte SOP token + 2 bytes header + 4*N bytes objects + 4 bytes CRC32
//
// The CRC has already been validated by FUSB302 auto-GoodCRC (any failure
// surfaces as the chip ignoring the message and never raising I_GCRCSENT);
// the trailing 4 bytes are read out and discarded.
//
// On an empty FIFO (STATUS1.RX_EMPTY=1) returns Ok with *out_received=false
// and consumes no FIFO bytes. On non-empty: *out_received=true and *out is
// populated. Caller should call this in a loop after I_GCRCSENT — multiple
// messages can stack between events.
//
// Receiving SOP'_DEBUG or SOP''_DEBUG tokens (we don't enable those code
// paths in v1) returns Internal as a "chip in unexpected state" signal.
UcsiPpmStatus ucsi_ppm_phy_recv_message(UcsiPpm* ppm, UcsiPpmPhyPdMsg* out, bool* out_received);

// --- Measurements (MDAC + BC_LVL) ------------------------------------------

// One-shot VBUS threshold check. Programs MEASURE with MEAS_VBUS=1 and the
// MDAC value closest to (and >=) `voltage_mv`, then reads STATUS0.COMP.
// `*above` is set to true if VBUS is currently above the threshold.
//
// Resolution is coarse — VBUS is divided by 10 inside the chip, so the
// effective MDAC step is 420 mV. The threshold rounds **up** to keep
// "above X mV" detections conservative (the actual threshold is >= X mV).
// Used for vSafe0V / vSafe5V detection (plan/fusb302.md §2.6).
UcsiPpmStatus ucsi_ppm_phy_measure_vbus_threshold(UcsiPpm* ppm, uint16_t voltage_mv, bool* above);

// Arms continuous VBUS threshold monitoring. Programs MEASURE the same way
// as `measure_vbus_threshold` but doesn't read STATUS0 — instead, I_COMP_CHNG
// fires whenever VBUS crosses the threshold in either direction (subsequent
// `pump` calls emit `UcsiPpmPhyEventCompChanged` with `comp_above`).
// Used by PE during Hard Reset / PR_Swap (plan/architecture.md §1 — direct
// PE → L4 access).
UcsiPpmStatus ucsi_ppm_phy_arm_vbus_compare(UcsiPpm* ppm, uint16_t voltage_mv);

// Reads STATUS0.BC_LVL (2-bit field). Used by Type-C SM after polarity lock
// to determine partner Rp current level for `Power Operation Mode` in
// GET_CONNECTOR_STATUS (plan/fusb302.md §5.2):
//   0b00 — < 200 mV (no Rd / not present)
//   0b01 — > 200 mV, < 660 mV  (USB Default 80 µA / 500 mA)
//   0b10 — > 660 mV, < 1.23 V  (1.5 A capability)
//   0b11 — > 1.23 V            (3 A capability)
// Caller must ensure SWITCHES0.MEAS_CC{1,2} is set on the active CC pin
// before calling.
UcsiPpmStatus ucsi_ppm_phy_read_bc_lvl(UcsiPpm* ppm, uint8_t* out_bc_lvl);

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
