#pragma once

#include <furi_hal_i2c_types.h>
#include <furi_hal_gpio.h>

#define BQ28Z620_ADDRESS 0x22

typedef struct Bq28z620 Bq28z620;

typedef enum {
    Bq28z620StatusOk = 0,
    Bq28z620StatusRxEmpty,
    Bq28z620StatusTxEmpty,
    Bq28z620StatusError = -1,
    Bq28z620StatusTimeout = -2,
    Bq28z620StatusUnknown = -3,
} Bq28z620Status;


#ifdef __cplusplus
extern "C" {
#endif

Bq28z620* bq28z620_init(const FuriHalI2cBusHandle* i2c_handle, uint8_t address);
void bq28z620_deinit(Bq28z620* instance);    


#ifdef __cplusplus
}
#endif
