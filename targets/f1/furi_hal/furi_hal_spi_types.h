#pragma once

/**
 * SPI channels
 */
typedef enum {
    FuriHalSpiIdSPI0 = 0,
    FuriHalSpiIdSPI1,

    FuriHalSpiIdMax,
} FuriHalSpiId;


typedef enum {
    FuriHalSpiPinSck,
    FuriHalSpiPinMosi,
    FuriHalSpiPinMiso,
    FuriHalSpiPinCs,

    FuriHalSpiPinMax,
} FuriHalSpiPin;

typedef enum {
    FuriHalSpiTransferBitOrderLsbFirst,
    FuriHalSpiTransferBitOrderMsbFirst,
} FuriHalSpiTransferBitOrder;

typedef enum {
    FuriHalSpiTransferMode0, // CPOL = 0, CPHA = 0
    FuriHalSpiTransferMode1, // CPOL = 0, CPHA = 1
    FuriHalSpiTransferMode2, // CPOL = 1, CPHA = 0
    FuriHalSpiTransferMode3, // CPOL = 1, CPHA = 1
} FuriHalSpiTransferMode;

typedef enum {
    FuriHalSpiModeMaster,
    FuriHalSpiModeSlave,
} FuriHalSpiMode;

typedef struct FuriHalSpiHandle FuriHalSpiHandle;
