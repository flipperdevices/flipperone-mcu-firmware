#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct DisplayJd9853SPI DisplayJd9853SPI;

#ifdef __cplusplus
extern "C" {
#endif

DisplayJd9853SPI* display_jd9853_spi_init(void);
void display_jd9853_spi_deinit(DisplayJd9853SPI* display);
void display_jd9853_spi_on_sleep_enter(void);
void display_jd9853_spi_on_sleep_exit(void);
void display_jd9853_spi_set_brightness(DisplayJd9853SPI* display, uint8_t brightness);
uint8_t display_jd9853_spi_get_brightness(DisplayJd9853SPI* display);
void display_jd9853_spi_write_buffer(DisplayJd9853SPI* display, uint16_t w, uint16_t h, const uint8_t* buffer, size_t size);
void display_jd9853_spi_write_buffer_x_y(DisplayJd9853SPI* display, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t* buffer, size_t size);
void display_jd9853_spi_fill(DisplayJd9853SPI* display, uint8_t color);
void display_jd9853_spi_eco_mode(DisplayJd9853SPI* display, bool enable);

#ifdef __cplusplus
}
#endif
