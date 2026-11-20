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
    CpuStateNumStates,
} CpuState;

/* MCU response to the host going down; wire values of the reset mode register */
typedef enum {
    HostResetResponseDisabled, /* Ignore host shutdown (default) */
    HostResetResponseResetMcu, /* Reset the MCU when the host goes down */
    HostResetResponseResetMcuAndPower, /* Also power-cycle the host */
    HostResetResponseCount,
} HostResetResponse;

#ifdef __cplusplus
extern "C" {
#endif

bool host_set_cpu_state(Host* instance, CpuState cpu_state);
bool host_get_cpu_state(Host* instance, CpuState* cpu_state);

/** Power the host unless it already is. */
void host_power_on(Host* instance);
/** Put PD and the charger back to defaults and cut the host rails. */
void host_power_off(Host* instance);
/** Cut the host rails and power it again. */
void host_power_restart(Host* instance);
/** Power the host into the SoC maskrom; the strap is released after a second. */
void host_power_maskrom(Host* instance);

/** Arm the MCU response to the host going down (RAM-only). */
bool host_set_reset_response(Host* instance, HostResetResponse response);
bool host_get_reset_response(Host* instance, HostResetResponse* response);
/** Delay between the triggering host state and the response, in 100 ms units. */
bool host_set_reset_delay(Host* instance, uint16_t reset_delay_100ms);
/** Flash storage offset of a freshly written partition, booted by an armed ResetMcu response (0 clears it). */
bool host_set_update_base(Host* instance, size_t update_base);
/** Host-requested MCU reset: activate a staged image if armed, else reset with a marker. Never returns. */
void host_reset_request(Host* instance);

#ifdef __cplusplus
}
#endif
