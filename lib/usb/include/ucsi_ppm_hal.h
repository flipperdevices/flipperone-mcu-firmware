#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>

#include "ucsi_ppm_errors.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t (*UcsiPpmTimeMsFn)(void* ctx);

typedef void (*UcsiPpmAlertFn)(void* ctx);

typedef UcsiPpmStatus (*UcsiPpmI2cReadFn)(
    void* ctx,
    uint8_t i2c_addr,
    uint8_t* data,
    size_t len);

typedef UcsiPpmStatus (*UcsiPpmI2cWriteFn)(
    void* ctx,
    uint8_t i2c_addr,
    const uint8_t* data,
    size_t len);

typedef bool (*UcsiPpmGpioReadFn)(void* ctx);

typedef void (*UcsiPpmGpioWriteFn)(void* ctx, bool value);

typedef UcsiPpmStatus (*UcsiPpmPowerSupplySetFn)(
    void* ctx,
    uint16_t voltage_mv,
    uint16_t current_limit_ma);

typedef bool (*UcsiPpmHasAltPowerFn)(void* ctx);

typedef enum {
    UcsiPpmLogLevelTrace,
    UcsiPpmLogLevelDebug,
    UcsiPpmLogLevelInfo,
    UcsiPpmLogLevelWarn,
    UcsiPpmLogLevelError,
} UcsiPpmLogLevel;

typedef void (*UcsiPpmLogFn)(
    void* ctx,
    UcsiPpmLogLevel level,
    const char* module,
    const char* fmt,
    va_list args);

#ifdef __cplusplus
}
#endif
