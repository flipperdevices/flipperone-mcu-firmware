#pragma once

#include <furi_hal_i2c_types.h>
#include <furi_hal_gpio.h>

#define TCA6416A_ADDRESS_A0 0x20
#define TCA6416A_ADDRESS_A1 0x21

typedef struct Tca6416a Tca6416a;
typedef void (*Tca6416aCallbackInput)(void* context);

#ifdef __cplusplus
extern "C" {
#endif

Tca6416a* tca6416a_init(const FuriHalI2cBusHandle* i2c_handle, const GpioPin* pin_reset, const GpioPin* pin_interrupt, uint8_t address);
void tca6416a_deinit(Tca6416a* instance);
void tca6416a_set_input_callback(Tca6416a* instance, Tca6416aCallbackInput callback, void* context);
bool tca6416a_write_output(Tca6416a* instance, uint16_t output_mask);
uint16_t tca6416a_read_input(Tca6416a* instance);
bool tca6416a_write_mode(Tca6416a* instance, uint16_t port_mask);
uint16_t tca6416a_read_mode(Tca6416a* instance);

#ifdef __cplusplus
}
#endif
