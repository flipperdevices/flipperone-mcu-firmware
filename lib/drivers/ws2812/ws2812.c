#include "ws2812.h"
#include "ws2812.pio.h"
#include <hardware/pio.h>

typedef struct {
    const GpioPin* pin;
    PIO pio;
    uint sm;
    uint offset;
} Ws2812Line;

struct Ws2812 {
    Ws2812Line* lines;
    size_t line_count;
};

Ws2812* ws2812_init(const GpioPin* pins, size_t line_count) {
    Ws2812* instance = malloc(sizeof(Ws2812));

    instance->lines = malloc(sizeof(Ws2812Line) * line_count);
    instance->line_count = line_count;

    for(size_t i = 0; i < line_count; i++) {
        instance->lines[i].pin = &pins[i];
        bool success = pio_claim_free_sm_and_add_program_for_gpio_range(
            &ws2812_program, &instance->lines[i].pio, &instance->lines[i].sm, &instance->lines[i].offset, pins[i].pin, 1, true);
        furi_check(success);

        ws2812_program_init(instance->lines[i].pio, instance->lines[i].sm, instance->lines[i].offset, pins[i].pin, 800000, false);
    }

    return instance;
}

void ws2812_deinit(Ws2812* instance) {
    furi_check(instance);
    for(size_t i = 0; i < instance->line_count; i++) {
        pio_sm_set_enabled(instance->lines[i].pio, instance->lines[i].sm, false);
        pio_remove_program_and_unclaim_sm(&ws2812_program, instance->lines[i].pio, instance->lines[i].offset, instance->lines[i].sm);
    }
    free(instance->lines);
    free(instance);
}

static FURI_ALWAYS_INLINE void ws2812_put_pixel(Ws2812* instance, size_t line_index, uint32_t pixel_grb) {
    furi_check(line_index <= instance->line_count);
    pio_sm_put_blocking(instance->lines[line_index].pio, instance->lines[line_index].sm, pixel_grb << 8u);
}

static FURI_ALWAYS_INLINE uint32_t ws2812_urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}

void ws2812_put_pixel_rgb(Ws2812* instance, size_t line_index, uint8_t r, uint8_t g, uint8_t b) {
    furi_check(line_index < instance->line_count);
    ws2812_put_pixel(instance, line_index, ws2812_urgb_u32(r, g, b));
}
