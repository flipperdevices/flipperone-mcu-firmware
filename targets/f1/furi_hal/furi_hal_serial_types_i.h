#pragma once

#include <stdbool.h>

#include <furi_hal_serial_types.h>

struct FuriHalSerialHandle {
    FuriHalSerialId id;
    bool in_use;
};
