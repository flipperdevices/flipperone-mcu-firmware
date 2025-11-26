#pragma once

#include <furi_hal_i2c_types.h>
#include <furi_hal_gpio.h>

#define IQS7211E_ADDRESS_A0 0x20
#define IQS7211E_ADDRESS_A1 0x21

typedef struct Iqs7211e Iqs7211e;
typedef void (*Iqs7211eCallbackInput)(void* context);

#ifdef __cplusplus
extern "C" {
#endif

Iqs7211e* iqs7211e_init(const FuriHalI2cBusHandle* i2c_handle, const GpioPin* pin_reset, const GpioPin* pin_interrupt, uint8_t address);
void iqs7211e_deinit(Iqs7211e* instance);
void iqs7211e_set_input_callback(Iqs7211e* instance, Iqs7211eCallbackInput callback, void* context);
bool iqs7211e_write_output(Iqs7211e* instance, uint16_t output_mask);
uint16_t iqs7211e_read_input(Iqs7211e* instance);
bool iqs7211e_write_mode(Iqs7211e* instance, uint16_t port_mask);
uint16_t iqs7211e_read_mode(Iqs7211e* instance);

#ifdef __cplusplus
}
#endif
