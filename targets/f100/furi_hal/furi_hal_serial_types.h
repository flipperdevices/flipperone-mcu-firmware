#pragma once

/**
 * UART channels
 */
typedef enum {
    FuriHalSerialIdUart0,
    FuriHalSerialIdUart1,

    FuriHalSerialIdMax,
} FuriHalSerialId;

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

typedef struct FuriHalSerialHandle FuriHalSerialHandle;
