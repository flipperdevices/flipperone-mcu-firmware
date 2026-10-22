#pragma once
#include <furi.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FuriHalLogOutputNone,
    FuriHalLogOutputRtt,
    FuriHalLogOutputSerial,
    FuriHalLogOutputAll,
} FuriHalLogOutput;

// call as soon as possible in the boot process to capture early logs
void furi_hal_log_init(FuriLogLevel level, FuriHalLogOutput output);

// call after hardware is ready to output logs
void furi_hal_log_hardware_init(void);

#ifdef __cplusplus
}
#endif