#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

typedef struct DisplayJd9853QSPI DisplayJd9853QSPI;

#ifdef __cplusplus
extern "C" {
#endif

DisplayJd9853QSPI* display_jd9853_qspi_init(void);
void display_jd9853_qspi_deinit(DisplayJd9853QSPI* display);
void display_jd9853_load_config(DisplayJd9853QSPI* display, const uint8_t* config);
void display_jd9853_qspi_on_sleep_enter(void);
void display_jd9853_qspi_on_sleep_exit(void);
void display_jd9853_qspi_set_brightness(DisplayJd9853QSPI* display, int8_t brightness);
int8_t display_jd9853_qspi_get_brightness(DisplayJd9853QSPI* display);
void display_jd9853_qspi_write_buffer(DisplayJd9853QSPI* display, const uint8_t* buffer, size_t size);
void display_jd9853_qspi_fill(DisplayJd9853QSPI* display, uint8_t color);
void display_jd9853_qspi_eco_mode(DisplayJd9853QSPI* display, bool enable);
void display_jd9853_qspi_set_vci(DisplayJd9853QSPI* display, float_t voltage);
float_t display_jd9853_qspi_get_vci(DisplayJd9853QSPI* display);

#ifdef __cplusplus
}
#endif
