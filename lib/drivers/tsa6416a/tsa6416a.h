#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <furi_hal_i2c_types.h>
#include <furi_hal_gpio.h>

#define TCA6416A_ADDRESS_A0 0x20
#define TCA6416A_ADDRESS_A1 0x21

typedef struct Tsa6416a Tsa6416a;
typedef void (*Tsa6416aCallbackInput)(void* context);

#ifdef __cplusplus
extern "C" {
#endif

Tsa6416a* tsa6416a_init(FuriHalI2cHandle* i2c_handle, const GpioPin* pin_reset, const GpioPin* pin_interrupt, uint8_t address);
void tsa6416a_deinit(Tsa6416a* instance);
void tsa6416a_set_input_callback(Tsa6416a* instance, Tsa6416aCallbackInput callback, void* context);
bool tsa6416a_write_output(Tsa6416a* instance, uint16_t output_mask);
uint16_t tsa6416a_read_input(Tsa6416a* instance);
bool tsa6416a_write_mode(Tsa6416a* instance, uint16_t port_mask);
uint16_t tsa6416a_read_mode(Tsa6416a* instance);

#ifdef __cplusplus
}
#endif
