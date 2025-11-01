#pragma once

#include <stdbool.h>

#include <furi_hal_i2c_types.h>

struct FuriHalI2cHandle {
    FuriHalI2cId id;
    bool in_use;
};
