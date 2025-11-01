#pragma once

/**
 * I2C channels
 */
typedef enum {
    FuriHalI2cIdI2c0 = 0,
    FuriHalI2cIdI2c1,

    FuriHalI2cIdMax,
} FuriHalI2cId;


typedef enum {
    FuriHalI2cPinSda,
    FuriHalI2cPinScl,

    FuriHalI2cPinMax,
} FuriHalI2cPin;

// typedef enum {
//     FuriHalI2cModeMaster,
//     FuriHalI2cModeSlave,
// } FuriHalI2cMode;

typedef struct FuriHalI2cHandle FuriHalI2cHandle;
