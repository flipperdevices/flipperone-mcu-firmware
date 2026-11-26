#pragma once
#include <furi.h>

#define RECORD_HOST "host"

/**
 * The host (the Linux CPU): its state as Linux reports it, its power, and the
 * MCU's response to it going down. The only place that switches its rails.
 */
typedef struct Host Host;

/* Wire values of the CPU state register, shared with the Linux driver */
typedef enum {
    CpuStateUnknown, /* Unknown CPU state */
    CpuStateBootloader, /* Set by the MCU/bootloader itself */
    CpuStateKernelInit, /* This driver has probed */
    CpuStateOnline, /* Userspace up / resumed from suspend */
    CpuStateSuspendReq, /* Userspace preparing to suspend */
    CpuStateSuspend, /* Kernel about to suspend */
    CpuStateRebootReq, /* Userspace preparing to reboot */
    CpuStatePoweroffReq, /* Userspace preparing to power off */
    CpuStateShuttingDown, /* Kernel reboot/power-off in progress */
    CpuStatePoweredOff, /* Safe to cut power to the PMIC */
    CpuStateRestarting, /* Safe to power-cycle the PMIC */
    CpuStateNumStates,
} CpuState;

/* MCU response to the host going down; wire values of the reset mode register */
typedef enum {
    HostResetResponseDisabled, /* Ignore host shutdown (default) */
    HostResetResponseResetMcu, /* Switch: cut host power and boot the staged image; without one, as Disabled */
    HostResetResponseCount,
} HostResetResponse;

/* The host as its screen shows it; published on host_get_pubsub() */
typedef enum {
    HostStateOff, /* Rails off */
    HostStateStarting, /* Powered from cold; stays until the next power change */
    HostStateRunning, /* Found powered after an MCU reset, or reported back after not responding */
    HostStateNotResponding, /* Powered, but Linux has not written its CPU state in time */
    HostStateMaskrom, /* Powered into the SoC maskrom */
} HostState;

/* Published on host_get_pubsub() when either state changes. Cutting the
 * host's power forgets its CPU state: CpuStateUnknown until it reports again. */
typedef struct {
    HostState state;
    CpuState cpu_state;
} HostEvent;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Power the host unless it already is, and hand it the main expander. A
 * running host is asked to send its screen again. Call with the frame
 * receiver up.
 */
void host_power_on(Host* instance);
/** Put PD and the charger back to defaults and cut the host rails. */
void host_power_off(Host* instance);
/** Cut the host rails and power it again. */
void host_power_restart(Host* instance);
/** Power the host into the SoC maskrom; the strap is released after a second. */
void host_power_maskrom(Host* instance);

HostState host_get_state(Host* instance);

/** Publishes a HostEvent on every change of HostState or CpuState, from the host thread. */
FuriPubSub* host_get_pubsub(Host* instance);

#ifdef __cplusplus
}
#endif
