#pragma once

#include <furi_hal_i2c_types.h>
#include <furi_hal_gpio.h>

#define FUSB302_ADDRESS 0x22

typedef struct Fusb302 Fusb302;
typedef void (*Fusb302CallbackInput)(void* context);

#ifdef __cplusplus
extern "C" {
#endif

// typedef enum{
//     Fusb302Range16V = 0b0,
//     Fusb302Range32V = 0b1,
// } Fusb302Range;

Fusb302* fusb302_init(const FuriHalI2cBusHandle* i2c_handle, uint8_t address, const GpioPin* pin_interrupt);
void fusb302_read_cc_status(Fusb302* instance, uint8_t cc);
void fusb302_deinit(Fusb302* instance);
bool fusb302_read_role(Fusb302* instance);
void fusb302_pd_reset(Fusb302* instance) ;
#ifdef __cplusplus
}
#endif
