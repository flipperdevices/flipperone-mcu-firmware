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

// Optional combined write-then-read in a single bus transaction
// (repeated-start, no STOP between). Use when the bus is shared with
// other drivers — without a combined transaction another driver can take
// the bus between i2c_write(reg) and i2c_read(data) and FUSB302 loses
// its register pointer, returning garbage from the wrong address. If
// NULL the lib falls back to two separate i2c_write + i2c_read calls.
typedef UcsiPpmStatus (*UcsiPpmI2cWriteReadFn)(
    void* ctx,
    uint8_t i2c_addr,
    const uint8_t* tx,
    size_t tx_len,
    uint8_t* rx,
    size_t rx_len);

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
