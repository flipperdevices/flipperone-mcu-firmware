#pragma once

/**
 * UART channels
 */
typedef enum {
    FuriHalSerialIdUart0,
    FuriHalSerialIdUart1,

    FuriHalSerialIdMax,
} FuriHalSerialId;

// typedef enum {
//     FuriHalSerialDirectionNone = 0,
//     FuriHalSerialDirectionTx = 1 << 0,
//     FuriHalSerialDirectionRx = 1 << 1,
//     FuriHalSerialDirectionTxRx = FuriHalSerialDirectionTx | FuriHalSerialDirectionRx,
// } FuriHalSerialDirection;

typedef enum {
    FuriHalSerialPinTx,
    FuriHalSerialPinRx,
    FuriHalSerialPinRts,
    FuriHalSerialPinCts,

    FuriHalSerialPinMax,
} FuriHalSerialPin;

typedef enum {
    FuriHalSerialConfigDataBits5,
    FuriHalSerialConfigDataBits6,
    FuriHalSerialConfigDataBits7,
    FuriHalSerialConfigDataBits8,
} FuriHalSerialConfigDataBits;

typedef enum {
    FuriHalSerialConfigParityNone,
    FuriHalSerialConfigParityEven,
    FuriHalSerialConfigParityOdd,
} FuriHalSerialConfigParity;

typedef enum {
    FuriHalSerialConfigStopBits_1,
    FuriHalSerialConfigStopBits_2,
} FuriHalSerialConfigStopBits;

// typedef enum {
//     FuriHalSerialTransferBitOrderLsbFirst,
//     FuriHalSerialTransferBitOrderMsbFirst,
// } FuriHalSerialTransferBitOrder;

// typedef enum {
//     FuriHalSerialBinaryDataLogicPositive,
//     FuriHalSerialBinaryDataLogicNegative,
// } FuriHalSerialBinaryDataLogic;

typedef struct FuriHalSerialHandle FuriHalSerialHandle;
