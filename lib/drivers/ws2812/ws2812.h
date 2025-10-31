#pragma once
#include <furi_hal_gpio.h>

typedef struct Ws2812 Ws2812;

#ifdef __cplusplus
extern "C" {
#endif

Ws2812* ws2812_init(const GpioPin* pins, size_t line_count);
void ws2812_deinit(Ws2812* instance);
void ws2812_put_pixel_rgb(Ws2812* instance, size_t line_index, uint8_t r, uint8_t g, uint8_t b);

#ifdef __cplusplus
}
#endif
