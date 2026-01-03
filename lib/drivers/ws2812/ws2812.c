#include "ws2812.h"
#include "ws2812.pio.h"
#include <hardware/pio.h>
#include <hardware/dma.h>
#include <pico/stdlib.h>

typedef struct {
    const GpioPin* pin;
    PIO pio;
    uint sm;
    uint offset;
} Ws2812Line;

struct Ws2812 {
    Ws2812Line* lines;
    size_t line_count;
    FuriSemaphore* dma_busy;
    uint32_t dma_tx_channel;
    alarm_id_t reset_delay_alarm_id;
};

static Ws2812* ws2812_instance = NULL;

static int64_t __isr __not_in_flash_func(reset_delay_complete)(__unused alarm_id_t id, __unused void* user_data) {
    ws2812_instance->reset_delay_alarm_id = 0;
    furi_semaphore_release(ws2812_instance->dma_busy);
    // no repeat
    return 0;
}

static void __isr __not_in_flash_func(dma_complete_handler)(void) {
    if(dma_channel_get_irq1_status(ws2812_instance->dma_tx_channel)) {
        // clear IRQ
        dma_channel_acknowledge_irq1(ws2812_instance->dma_tx_channel);
        // when the dma is complete we start the reset delay timer
        if(ws2812_instance->reset_delay_alarm_id) cancel_alarm(ws2812_instance->reset_delay_alarm_id);
        ws2812_instance->reset_delay_alarm_id = add_alarm_in_us(400, reset_delay_complete, NULL, true);
    }
}

Ws2812* ws2812_init(const GpioPin* pins, size_t line_count) {
    furi_check(!ws2812_instance); //only one instance allowed
    Ws2812* instance = malloc(sizeof(Ws2812));

    instance->lines = malloc(sizeof(Ws2812Line) * line_count);
    instance->line_count = line_count;
    instance->dma_busy = furi_semaphore_alloc(1, 1);

    for(size_t i = 0; i < line_count; i++) {
        instance->lines[i].pin = &pins[i];
        bool success = pio_claim_free_sm_and_add_program_for_gpio_range(
            &ws2812_program, &instance->lines[i].pio, &instance->lines[i].sm, &instance->lines[i].offset, pins[i].pin, 1, true);
        furi_check(success);

        ws2812_program_init(instance->lines[i].pio, instance->lines[i].sm, instance->lines[i].offset, pins[i].pin, 800000, false);
    }

    //dma init
    instance->dma_tx_channel = dma_claim_unused_channel(true);
    furi_check(dma_channel_is_claimed(instance->dma_tx_channel));

    irq_set_exclusive_handler(DMA_IRQ_1, dma_complete_handler);
    dma_channel_set_irq1_enabled(instance->dma_tx_channel, true);
    irq_set_enabled(DMA_IRQ_1, true);

    ws2812_instance = instance;

    return instance;
}

void ws2812_deinit(Ws2812* instance) {
    furi_check(instance);
    irq_set_enabled(DMA_IRQ_1, false);
    dma_channel_unclaim(instance->dma_tx_channel);
    furi_semaphore_free(instance->dma_busy);

    for(size_t i = 0; i < instance->line_count; i++) {
        pio_sm_set_enabled(instance->lines[i].pio, instance->lines[i].sm, false);
        pio_remove_program_and_unclaim_sm(&ws2812_program, instance->lines[i].pio, instance->lines[i].offset, instance->lines[i].sm);
    }

    free(instance->lines);
    free(instance);
    ws2812_instance = NULL;
}

static FURI_ALWAYS_INLINE void ws2812_put_pixel(Ws2812* instance, size_t line_index, uint32_t pixel_grb) {
    furi_check(line_index <= instance->line_count);
    pio_sm_put_blocking(instance->lines[line_index].pio, instance->lines[line_index].sm, pixel_grb);
}

FURI_ALWAYS_INLINE uint32_t ws2812_urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)(r) << 16) | ((uint32_t)(g) << 24) | (uint32_t)(b << 8);
}

void ws2812_put_pixel_rgb(Ws2812* instance, size_t line_index, uint8_t r, uint8_t g, uint8_t b) {
    furi_check(line_index < instance->line_count);
    ws2812_put_pixel(instance, line_index, ws2812_urgb_u32(r, g, b));
}

void ws2812_write_buffer_dma(Ws2812* instance, size_t line_index, const uint32_t* buffer, size_t size_per_line) {
    furi_check(instance);
    furi_check(buffer);
    furi_check(size_per_line > 0);

    furi_check(furi_semaphore_acquire(instance->dma_busy, FuriWaitForever) == FuriStatusOk);
    dma_channel_config c = dma_channel_get_default_config(instance->dma_tx_channel);
    channel_config_set_dreq(&c, pio_get_dreq(instance->lines[line_index].pio, instance->lines[line_index].sm, true));
    dma_channel_set_config(instance->dma_tx_channel, &c, false);

    dma_channel_set_write_addr(instance->dma_tx_channel, &instance->lines[line_index].pio->txf[instance->lines[line_index].sm], false);
    dma_channel_set_read_addr(instance->dma_tx_channel, buffer, false);
    dma_channel_set_transfer_count(instance->dma_tx_channel, size_per_line, false);

    // Start DMA transfer
    dma_channel_start(instance->dma_tx_channel);
}
