#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void put_pixel_rgb(uint8_t r, uint8_t g, uint8_t b);

void init(uint32_t gpio);

#ifdef __cplusplus
}
#endif
