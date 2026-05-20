#pragma once

// L3 PE (Policy Engine) — PD R3.0 §8 state machines for the connector.
// v1 implements the Sink contract path:
//   Idle → WaitForCapabilities → WaitForAccept → WaitForPsRdy → Ready
// Source path, PR_Swap, DR_Swap and Hard Reset orchestration are TODO.

#include "ucsi_ppm.h"
#include "ucsi_ppm_i.h"
#include "ucsi_ppm_phy.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UcsiPpmPeStateIdle, // not attached or contract torn down
    UcsiPpmPeSnkWaitForCapabilities, // armed SinkWaitCapTimer
    UcsiPpmPeSnkWaitForAccept, // Request sent, armed SenderResponseTimer
    UcsiPpmPeSnkWaitForPsRdy, // Accept received, armed PSTransitionTimer
    UcsiPpmPeSnkReady, // explicit PD contract held (sink)
    UcsiPpmPeSrcSendCapabilities, // Source_Capabilities sent, awaiting Request
    UcsiPpmPeSrcTransitionSupply, // Request accepted, PSU ramping, armed PSTransitionTimer
    UcsiPpmPeSrcReady, // explicit PD contract held (source)
    UcsiPpmPePendingHardResetSent, // Hard Reset triggered, waiting for HARDSENT
    UcsiPpmPeWaitForSoftResetAccept, // Soft_Reset sent, armed SenderResponseTimer
    UcsiPpmPeWaitForDrSwapResponse, // DR_Swap sent, armed SenderResponseTimer
    UcsiPpmPeStateError, // unrecoverable: HardResetCounter exhausted
} UcsiPpmPeState;

UcsiPpmStatus ucsi_ppm_pe_init(UcsiPpm* ppm);
UcsiPpmStatus ucsi_ppm_pe_reset(UcsiPpm* ppm);

// TC notifies PE on attach / detach. attach_snk arms SinkWaitCapTimer;
// attach_src starts the Source_Capabilities retransmission loop; detach
// drops PE back to Idle and clears any half-formed contract.
void ucsi_ppm_pe_on_attach_snk(UcsiPpm* ppm);
void ucsi_ppm_pe_on_attach_src(UcsiPpm* ppm);
void ucsi_ppm_pe_on_detach(UcsiPpm* ppm);

// L2 notifies PE that the external PSU has settled at the requested
// voltage/current (api.md §5.4 notify_power_supply_ready). In source-mode
// SrcTransitionSupply this drives the PS_RDY emission and contract commit.
void ucsi_ppm_pe_on_power_supply_ready(UcsiPpm* ppm);

// PHY-pump event hook. PE cares about HardResetSent (our Hard Reset just
// went out — recover the state machine) and HardResetRx (partner Hard Reset
// arrived — reset and wait for new Caps/Request). Other event kinds are
// ignored — TC/PRL handle them.
void ucsi_ppm_pe_handle_phy_event(UcsiPpm* ppm, const UcsiPpmPhyEvent* event);

// Initiates a DR_Swap toward the partner (PD R3.0 §6.3.10 / §8.3.3.8). Valid
// only from SnkReady or SrcReady. If `to_dfp` already matches our current
// data role this is a no-op (returns Ok). On success the PE moves to
// WaitForDrSwapResponse and arms SenderResponseTimer; partner's Accept flips
// the data role, Reject/Wait/timeout leaves the role intact.
UcsiPpmStatus ucsi_ppm_pe_request_dr_swap(UcsiPpm* ppm, bool to_dfp);

// Renegotiates the existing sink-side contract at a different operating
// current. Builds a fresh Request RDO selecting the same PDO position as
// the prior contract, ships it via PRL, and re-enters the Accept→PS_RDY
// flow. Returns InvalidArg if we're not Attached.SNK in PE_SNK_Ready or
// if we have no cached Source_Capabilities to renegotiate against.
// Driven by L2 SET_POWER_LEVEL on the sink direction.
UcsiPpmStatus ucsi_ppm_pe_request_renegotiate(UcsiPpm* ppm, uint16_t operating_current_ma);

// PRL hands every deduplicated SOP message here. PE inspects msg_type and
// advances the state machine. Non-SOP messages are dropped.
void ucsi_ppm_pe_handle_message(UcsiPpm* ppm, const UcsiPpmPhyPdMsg* msg);

// Periodic timer-tick — called from ucsi_ppm_tick after TC tick. Drives
// SenderResponseTimer / PSTransitionTimer / SinkWaitCapTimer expiry.
void ucsi_ppm_pe_tick(UcsiPpm* ppm);

#ifdef __cplusplus
}
#endif
