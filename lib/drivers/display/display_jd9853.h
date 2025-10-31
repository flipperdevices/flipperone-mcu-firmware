#pragma once
#include <stdint.h>
#include <stddef.h>

typedef struct DisplayJd9853 DisplayJd9853;

#ifdef __cplusplus
extern "C" {
#endif

DisplayJd9853* display_jd9853_init(void);
void display_jd9853_write_buffer(DisplayJd9853* display, uint16_t w, uint16_t h, const uint8_t* buffer, size_t size);
void display_jd9853_fill(DisplayJd9853* display, uint8_t r, uint8_t g, uint8_t b);

#ifdef __cplusplus
}
#endif
