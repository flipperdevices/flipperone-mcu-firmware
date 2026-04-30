#pragma once

#include <furi_hal_i2c_types.h>
#include <furi_hal_gpio.h>
#include "hd3ss3220_reg.h"

#define HD3SS3220_ADDRESS 0x47

typedef struct Hd3ss3220 Hd3ss3220;
typedef void (*Hd3ss3220CallbackInput)(void* context);

typedef enum {
    Hd3ss3220StatusUnknown = 0,
    Hd3ss3220StatusOk = 1,
    Hd3ss3220StatusError = -1,
    Hd3ss3220StatusTimeout = -2,
} Hd3ss3220Status;

#ifdef __cplusplus
extern "C" {
#endif

Hd3ss3220* hd3ss3220_init(const FuriHalI2cBusHandle* i2c_handle, uint8_t address, const GpioPin* pin_interrupt);
void hd3ss3220_deinit(Hd3ss3220* instance);
void hd3ss3220_set_input_callback(Hd3ss3220* instance, Hd3ss3220CallbackInput callback, void* context);

#ifdef __cplusplus
}
#endif
