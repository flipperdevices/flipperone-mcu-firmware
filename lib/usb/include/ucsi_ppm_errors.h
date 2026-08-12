#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UcsiPpmStatusOk = 0,
    UcsiPpmStatusInvalidArg,
    UcsiPpmStatusInvalidConfig,
    UcsiPpmStatusNotInitialized,
    UcsiPpmStatusAlreadyInitialized,
    UcsiPpmStatusBusy,
    UcsiPpmStatusHalError,
    UcsiPpmStatusInternal,
} UcsiPpmStatus;

#ifdef __cplusplus
}
#endif
