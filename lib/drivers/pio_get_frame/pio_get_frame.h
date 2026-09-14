#pragma once
#include <furi_hal_gpio.h>

typedef struct PioGetFrame PioGetFrame;
typedef void (*PioGetFrameCallbackRx)(uint8_t*data, size_t size, void* context);

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the PIO frame reader.
 *
 * @param gpio_cs   frame select pin; rising edge = end of frame
 * @param gpio_sck  external SPI clock pin
 * @param gpio_data data pin (external master MOSI)
 */
PioGetFrame* pio_get_frame_init(const GpioPin* gpio_cs, const GpioPin* gpio_sck, const GpioPin* gpio_data);
void pio_get_frame_deinit(PioGetFrame* instance);
void pio_get_frame_set_callback_rx(PioGetFrame* instance, PioGetFrameCallbackRx callback, void* context);

#ifdef __cplusplus
}
#endif
