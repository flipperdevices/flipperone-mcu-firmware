#include "ws2812.h"
#include "ws2812.pio.h"

static PIO pio;
static uint sm;
static uint offset;

static inline void put_pixel(PIO pio, uint sm, uint32_t pixel_grb) {
    pio_sm_put_blocking(pio, sm, pixel_grb << 8u);
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}

void ws2812_put_pixel_rgb(uint8_t r, uint8_t g, uint8_t b) {
    put_pixel(pio, sm, urgb_u32(r, g, b));
}

void ws2812_init(uint32_t gpio) {
    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&ws2812_program, &pio, &sm, &offset, gpio, 1, true);
    hard_assert(success);

    ws2812_program_init(pio, sm, offset, gpio, 800000, false);
}
