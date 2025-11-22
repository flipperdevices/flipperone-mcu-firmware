#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct DisplayJd9853 DisplayJd9853;

#ifdef __cplusplus
extern "C" {
#endif

DisplayJd9853* display_jd9853_init(void);
void display_jd9853_deinit(DisplayJd9853* display);
void display_jd9853_hstx_clock_init(void);
void display_jd9853_backlight_set_brightness(DisplayJd9853* display, uint8_t brightness);
uint8_t display_jd9853_backlight_get_brightness(DisplayJd9853* display);
void display_jd9853_write_buffer(DisplayJd9853* display, const uint8_t* buffer, size_t size);
void display_jd9853_fill(DisplayJd9853* display, uint8_t color);
void display_jd9853_eco_mode(DisplayJd9853* display, bool enable);

#ifdef __cplusplus
}
#endif
