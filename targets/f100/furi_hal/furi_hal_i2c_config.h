#pragma once

#include <furi_hal_i2c_types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern FuriHalI2cBus furi_hal_i2c_bus_internal;

extern FuriHalI2cBus furi_hal_i2c_bus_external;

extern const FuriHalI2cBusHandle furi_hal_i2c_handle_internal;

extern const FuriHalI2cBusHandle furi_hal_i2c_handle_external;

#ifdef __cplusplus
}
#endif
