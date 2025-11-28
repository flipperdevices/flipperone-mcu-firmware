#pragma once

#include <furi_hal_i2c_types.h>
#include <furi_hal_gpio.h>

#define IQS7211E_ADDRESS 0x56

typedef struct Iqs7211e Iqs7211e;
typedef void (*Iqs7211eCallbackInput)(void* context);

#ifdef __cplusplus
extern "C" {
#endif

Iqs7211e* iqs7211e_init(const FuriHalI2cBusHandle* i2c_handle, const GpioPin* pin_rdy, uint8_t address);
void iqs7211e_deinit(Iqs7211e* instance);
void iqs7211e_set_input_callback(Iqs7211e* instance, Iqs7211eCallbackInput callback, void* context);
#ifdef __cplusplus
}
#endif
