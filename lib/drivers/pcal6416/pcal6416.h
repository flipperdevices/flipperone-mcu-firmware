#pragma once

#include <furi_hal_i2c_types.h>
#include <furi_hal_gpio.h>

#define PCAL6416_ADDRESS_A0 0x20
#define PCAL6416_ADDRESS_A1 0x21

typedef struct Pcal6416 Pcal6416;
typedef void (*Pcal6416CallbackInput)(void* context);

typedef enum {
    Pcal6416StatusUnknown = 0,
    Pcal6416StatusOk = 1,
    Pcal6416StatusError = -1,
    Pcal6416StatusTimeout = -2,
} Pcal6416Status;

#ifdef __cplusplus
extern "C" {
#endif

Pcal6416* pcal6416_init(const FuriHalI2cBusHandle* i2c_handle, const GpioPin* pin_reset, const GpioPin* pin_interrupt, uint8_t address);
void pcal6416_deinit(Pcal6416* instance);
void pcal6416_set_input_callback(Pcal6416* instance, Pcal6416CallbackInput callback, void* context);
bool pcal6416_write_output(Pcal6416* instance, uint16_t output_mask);
uint16_t pcal6416_read_input(Pcal6416* instance);
bool pcal6416_write_mode(Pcal6416* instance, uint16_t port_mask);
uint16_t pcal6416_read_mode(Pcal6416* instance);

#ifdef __cplusplus
}
#endif
