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
    HostResetResponseResetMcu, /* Cut host power, reset the MCU into a staged image if any */
    HostResetResponseCount,
} HostResetResponse;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Register the host's I2C registers: CPU state (0x0040), system reset mode
 * and delay (0x0092, 0x0094) and the system interrupt (0x0104/0x0184), and raise its
 * restart bit. Once, before i2c_intercom_setup_end().
 */
void host_i2c_setup(Host* instance);

/**
 * Power the host unless it already is, and hand it the main expander. A host
 * that was already powered is running: it is asked to send its screen again.
 * Call with the frame receiver up.
 *
 * @return true when the host was already powered
 */
bool host_power_on(Host* instance);
/** Put PD and the charger back to defaults and cut the host rails. */
void host_power_off(Host* instance);
/** Cut the host rails and power it again. */
void host_power_restart(Host* instance);
/** Power the host into the SoC maskrom; the strap is released after a second. */
void host_power_maskrom(Host* instance);

#ifdef __cplusplus
}
#endif
