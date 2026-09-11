#pragma once
#include <furi.h>

#define RECORD_CPU_MODE "cpu_mode"

typedef struct CpuMode CpuMode;

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

#ifdef __cplusplus
extern "C" {
#endif

bool cpu_mode_set_cpu_mode(CpuMode* instance, CpuState cpu_state);
bool cpu_mode_get_cpu_mode(CpuMode* instance, CpuState* cpu_state);

#ifdef __cplusplus
}
#endif
