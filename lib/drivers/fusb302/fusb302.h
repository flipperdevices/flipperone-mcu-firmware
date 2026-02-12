#pragma once

#include <furi_hal_i2c_types.h>
#include <furi_hal_gpio.h>

#define FUSB302_ADDRESS 0x22

typedef struct Fusb302 Fusb302;
typedef void (*Fusb302Callback)(void* context);

#ifdef __cplusplus
extern "C" {
#endif

Fusb302* fusb302_init(const FuriHalI2cBusHandle* i2c_handle, uint8_t address, const GpioPin* pin_interrupt);
void fusb302_read_cc_status(Fusb302* instance, uint8_t cc);
void fusb302_deinit(Fusb302* instance);
bool fusb302_read_role(Fusb302* instance);
void fusb302_set_input_callback(Fusb302* instance, Fusb302Callback callback, void* context);
#ifdef __cplusplus
}
#endif
