#pragma once

//#include <furi_hal_spi_config.h>
//#include <stdbool.h>
#include <furi_hal_spi_types.h>

#ifdef __cplusplus
extern "C" {
#endif

void furi_hal_spi_init(
    FuriHalSpiHandle* handle,
    uint32_t baud_rate,
    FuriHalSpiTransferMode transfer_mode,
    FuriHalSpiTransferBitOrder bit_order,
    FuriHalSpiMode mode);
void furi_hal_spi_deinit(FuriHalSpiHandle* handle);
void furi_hal_spi_tx_blocking(FuriHalSpiHandle* handle, const uint8_t* tx_buffer, size_t size);

#ifdef __cplusplus
}
#endif
