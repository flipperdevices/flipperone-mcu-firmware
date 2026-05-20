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
    UcsiPpmPeSnkReady, // explicit PD contract held
    UcsiPpmPeStateError, // protocol violation / timeout (Hard Reset comes later)
} UcsiPpmPeState;

UcsiPpmStatus ucsi_ppm_pe_init(UcsiPpm* ppm);
UcsiPpmStatus ucsi_ppm_pe_reset(UcsiPpm* ppm);

// TC notifies PE on attach / detach. attach_snk arms SinkWaitCapTimer;
// detach drops PE back to Idle and clears any half-formed contract.
void ucsi_ppm_pe_on_attach_snk(UcsiPpm* ppm);
void ucsi_ppm_pe_on_detach(UcsiPpm* ppm);

// PRL hands every deduplicated SOP message here. PE inspects msg_type and
// advances the state machine. Non-SOP messages are dropped.
void ucsi_ppm_pe_handle_message(UcsiPpm* ppm, const UcsiPpmPhyPdMsg* msg);

// Periodic timer-tick — called from ucsi_ppm_tick after TC tick. Drives
// SenderResponseTimer / PSTransitionTimer / SinkWaitCapTimer expiry.
void ucsi_ppm_pe_tick(UcsiPpm* ppm);

#ifdef __cplusplus
}
#endif
