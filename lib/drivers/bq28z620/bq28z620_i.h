#pragma once

#include "bq28z620.h"

//#define BQ28Z620_DEBUG_ENABLE

#ifdef BQ28Z620_DEBUG_ENABLE
#define BQ28Z620_DEBUG(...) FURI_LOG_D(__VA_ARGS__)
#else
#define BQ28Z620_DEBUG(...)
#endif

#define BQ28Z620_DEFAULT_UNSEAL_KEY      (0x36720414U)
#define BQ28Z620_DEFAULT_FULL_ACCESS_KEY (0xFFFFFFFFU)

struct Bq28z620 {
    const FuriHalI2cBusHandle* i2c_handle;
    uint8_t address;
    void* context;
};
