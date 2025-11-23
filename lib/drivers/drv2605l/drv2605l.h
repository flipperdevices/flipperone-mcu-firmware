#pragma once

#include <furi_hal_i2c_types.h>
#include <furi_hal_gpio.h>

#define DRV2605L_ADDRESS (0x5Au)

typedef struct Drv2605l Drv2605l;
typedef void (*Drv2605lCallbackInput)(void* context);

#ifdef __cplusplus
extern "C" {
#endif

Drv2605l* drv2605l_init(const FuriHalI2cBusHandle* i2c_handle, const GpioPin* pin_reset, const GpioPin* pin_interrupt, uint8_t address);
void drv2605l_deinit(Drv2605l* instance);
void drv2605l_set_input_callback(Drv2605l* instance, Drv2605lCallbackInput callback, void* context);
bool drv2605l_write_output(Drv2605l* instance, uint16_t output_mask);
uint16_t drv2605l_read_input(Drv2605l* instance);
bool drv2605l_write_mode(Drv2605l* instance, uint16_t port_mask);
uint16_t drv2605l_read_mode(Drv2605l* instance);

#ifdef __cplusplus
}
#endif
