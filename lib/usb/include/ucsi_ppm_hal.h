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

// Where the sink current limit came from. The distinction matters to anyone
// deciding how much to trust the number: only TypeCOnly means "this is all we
// will ever learn about this partner".
typedef enum {
    // Nothing to draw: detached, sourcing, or no Rp on CC.
    UcsiPpmSinkLimitNone,
    // The source's Type-C Rp advertisement, with PD negotiation still
    // possible or in flight. Treat as a hard ceiling — exceeding it now is
    // what browns a source out mid-negotiation and gets us Hard Reset.
    UcsiPpmSinkLimitTypeC,
    // Same advertisement, but the partner has been shown not to speak PD, so
    // no better number is coming. Safe to probe for the real capability here
    // if the hardware can — nobody is negotiating any more.
    UcsiPpmSinkLimitTypeCOnly,
    // An explicit PD contract. Exact, and the partner is holding itself to
    // it: do not exceed it and do not go looking for more.
    UcsiPpmSinkLimitPdContract,
} UcsiPpmSinkLimitSource;

// How much current we are allowed to draw from the partner while attached as
// a sink. Called whenever the answer changes, and only then.
//
// Type-C R2.0 §4.6.2 forbids a sink from exceeding the Rp advertisement
// before a contract exists, so the integrator must apply the lower number
// promptly.
typedef void (*UcsiPpmSinkCurrentLimitFn)(void* ctx, uint16_t current_ma, UcsiPpmSinkLimitSource source);

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
