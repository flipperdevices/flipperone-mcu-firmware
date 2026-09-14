#pragma once

#include <stdbool.h>

#include <furi_hal_spi_types.h>

struct FuriHalSpiHandle {
    FuriHalSpiId id;
    bool in_use;
};
