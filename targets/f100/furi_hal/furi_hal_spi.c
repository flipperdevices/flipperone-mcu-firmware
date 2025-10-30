#include "core/check.h"
#include "furi_hal_spi_types.h"
#include <furi.h>
#include <furi_hal_spi.h>
#include <furi_hal_spi_types_i.h>

#include <furi_hal_resources.h>
#include <furi_hal_gpio.h>
#include <furi_hal_dma.h>

#include "hardware/spi.h"
#include "hardware/dma.h"
#include "pico/binary_info.h"
#include <stdint.h>

typedef struct {
    FuriHalSpiHandle* handle;
    spi_inst_t* periph_ptr;
    // FuriHalSpiRxCallback rx_callback;
    // FuriHalSpiTxCallback tx_callback;
    void* callback_context;
    uint32_t dma_rx_channel;
    uint32_t dma_tx_channel;
} FuriHalSpi;

typedef struct {
    spi_inst_t* periph;
    GpioAltFn alt_fn;
    const GpioPin* gpio[FuriHalSpiPinMax];
} FuriHalSpiResources;

static const FuriHalSpiResources furi_hal_spi_resources[FuriHalSpiIdMax] = {
    {
        .periph = spi0,
        .alt_fn = GpioAltFn1Spi,
        .gpio =
            {
                &gpio_display_scl, // SCK
                &gpio_display_sda, // MOSI
                NULL, // MISO
                &gpio_display_cs, // CS
            },
    },
    {
        .periph = spi1,
        .alt_fn = GpioAltFn1Spi,
        .gpio =
            {
                NULL, // SCK
                NULL, // MOSI
                NULL, // MISO
                NULL, // CS
            },
    },
};

static FuriHalSpi* furi_hal_spi[FuriHalSpiIdMax];

void furi_hal_spi_init(
    FuriHalSpiHandle* handle,
    uint32_t baud_rate,
    FuriHalSpiTransferMode transfer_mode,
    FuriHalSpiTransferBitOrder bit_order,
    FuriHalSpiMode mode) {
    furi_check(handle);
    const FuriHalSpiId spi_id = handle->id;
    furi_check(furi_hal_spi[spi_id] == NULL);

    furi_hal_spi[spi_id] = malloc(sizeof(FuriHalSpi));

    FuriHalSpi* spi = furi_hal_spi[spi_id];
    spi_inst_t* periph = furi_hal_spi_resources[spi_id].periph;
    spi->periph_ptr = periph;
    spi->handle = handle;

    // Initialize SPI peripheral
    spi_init(periph, baud_rate);
    if(mode == FuriHalSpiModeSlave) {
        spi_set_slave(periph, true);
    }

    // Configure SPI format
    spi_order_t order = (bit_order == FuriHalSpiTransferBitOrderMsbFirst) ? SPI_MSB_FIRST : SPI_LSB_FIRST;
    spi_cpol_t cpol, cpha;
    switch(transfer_mode) {
    case FuriHalSpiTransferMode0:
        cpol = SPI_CPOL_0;
        cpha = SPI_CPHA_0;
        break;
    case FuriHalSpiTransferMode1:
        cpol = SPI_CPOL_0;
        cpha = SPI_CPHA_1;
        break;
    case FuriHalSpiTransferMode2:
        cpol = SPI_CPOL_1;
        cpha = SPI_CPHA_0;
        break;
    case FuriHalSpiTransferMode3:
        cpol = SPI_CPOL_1;
        cpha = SPI_CPHA_1;
        break;
    default:
        furi_crash("Invalid SPI transfer mode");
        break;
    }
    spi_set_format(periph, 8, cpol, cpha, order);

    // Initialize GPIOs
    for(size_t i = 0; i < FuriHalSpiPinMax; i++) {
        const GpioPin* gpio = furi_hal_spi_resources[spi_id].gpio[i];
        if(gpio != NULL) {
            if(gpio == furi_hal_spi_resources[spi_id].gpio[FuriHalSpiPinCs]) {
                furi_hal_gpio_init_ex(gpio, GpioModeOutputPushPull, GpioPullNo, GpioSpeedFast, GpioAltFn5Sio);
            } else {
                furi_hal_gpio_init_ex(gpio, GpioModeOutputPushPull, GpioPullNo, GpioSpeedFast, furi_hal_spi_resources[spi_id].alt_fn);
            }
            furi_hal_gpio_set_drive_strength(gpio, GpioDriveStrengthHigh);
        }
    }

    //Initialize DMA channels
    //furi_check(furi_hal_dma_allocate_channel(spi->dma_rx_channel));
    //furi_check(furi_hal_dma_allocate_channel(spi->dma_tx_channel));
    furi_hal_dma_allocate_channel(&spi->dma_tx_channel);

    dma_channel_config c = dma_channel_get_default_config(spi->dma_tx_channel);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_dreq(&c, spi_get_dreq(periph, true));
    dma_channel_set_write_addr(spi->dma_tx_channel, &spi_get_hw(periph)->dr, false);
    dma_channel_set_config(spi->dma_tx_channel, &c, false);
}

void furi_hal_spi_deinit(FuriHalSpiHandle* handle) {
    furi_check(handle);
    const FuriHalSpiId spi_id = handle->id;
    furi_check(furi_hal_spi[spi_id]);

    FuriHalSpi* spi = furi_hal_spi[spi_id];
    spi_inst_t* periph = spi->periph_ptr;

    // Deinitialize SPI peripheral
    spi_deinit(periph);

    // Deinitialize GPIOs
    for(size_t i = 0; i < FuriHalSpiPinMax; i++) {
        const GpioPin* gpio = furi_hal_spi_resources[spi_id].gpio[i];
        if(gpio != NULL) {
            furi_hal_gpio_init_ex(gpio, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
        }
    }

    // Free DMA channels
    furi_hal_dma_free_channel(spi->dma_tx_channel);
    // furi_hal_dma_free_channel(spi->dma_rx_channel);

    free(spi);
    furi_hal_spi[spi_id] = NULL;
}

void furi_hal_spi_tx_blocking(FuriHalSpiHandle* handle, const uint8_t* tx_buffer, size_t size) {
    furi_check(handle);
    const FuriHalSpiId spi_id = handle->id;
    furi_check(furi_hal_spi[spi_id]);

    FuriHalSpi* spi = furi_hal_spi[spi_id];
    spi_inst_t* periph = spi->periph_ptr;

    dma_channel_set_read_addr(spi->dma_tx_channel, tx_buffer, false);
    dma_channel_set_transfer_count(spi->dma_tx_channel, size, false);

    // Cs low
    const GpioPin* cs_gpio = furi_hal_spi_resources[spi_id].gpio[FuriHalSpiPinCs];
    furi_hal_gpio_write(cs_gpio, false);

    // Start DMA transfer
    dma_start_channel_mask(1u << spi->dma_tx_channel);

    // Wait for DMA transfer to complete
    dma_channel_wait_for_finish_blocking(spi->dma_tx_channel);
    if(dma_channel_is_busy(spi->dma_tx_channel)) {
        panic("TX completed");
    }
    while (spi_is_busy(periph)) {
    
    }
    // Cs high
    furi_hal_gpio_write(cs_gpio, true);
}
