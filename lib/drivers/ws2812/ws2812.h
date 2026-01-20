#pragma once
#include <furi_hal_gpio.h>

typedef struct Ws2812 Ws2812;

#ifdef __cplusplus
extern "C" {
#endif

Ws2812* ws2812_init(const GpioPin* pins, size_t line_count);
void ws2812_deinit(Ws2812* instance);
void ws2812_put_pixel_rgb(Ws2812* instance, size_t line_index, uint8_t r, uint8_t g, uint8_t b); // Blocking version do not use will be deleted
uint32_t ws2812_urgb_u32(uint8_t r, uint8_t g, uint8_t b);
void ws2812_write_buffer_dma(Ws2812* instance, size_t line_index, const uint32_t* buffer, size_t size_per_line);
#ifdef __cplusplus
}
#endif
