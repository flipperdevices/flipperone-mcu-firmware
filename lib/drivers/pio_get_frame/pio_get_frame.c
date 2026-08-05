#include "pio_get_frame.h"
#include "pio_get_frame.pio.h"

#include <furi.h>
#include <furi_hal_gpio.h>
#include <furi_hal_resources.h>
#include <drivers/display/display_jd9853_reg.h>

#include <hardware/pio.h>
#include <hardware/dma.h>

#define TAG "PioGetFrame"

#define PIO_GET_FRAME_SIZE  (JD9853_WIDTH * JD9853_HEIGHT)
#define PIO_GET_FRAME_COUNT 2

typedef struct {
    uint8_t data[PIO_GET_FRAME_SIZE];
} PioGetFrameBuffer;

struct PioGetFrame {
    PIO pio;
    uint sm;
    uint offset;
    PioGetFrameBuffer frame_buffers[PIO_GET_FRAME_COUNT];
    size_t current_frame; /* buffer DMA is (re)armed to write into */
    int dma_rx_channel;
    PioGetFrameCallbackRx callback_rx;
    void* callback_context;
};

static PioGetFrame* pio_get_frame_instance = NULL;

static GpioAltFn pio_get_frame_pio_altfn(PIO pio) {
    if(pio == pio0) return GpioAltFn6Pio0;
    if(pio == pio1) return GpioAltFn7Pio1;
    return GpioAltFn8Pio2;
}

/* CS rising edge = frame complete. Hand the filled buffer to the callback and
 * re-arm DMA into the other double buffer for the next frame. */
static void __isr __not_in_flash_func(pio_get_frame_cs_isr)(void* context) {
    PioGetFrame* instance = (PioGetFrame*)context;
    if(!instance) return;

    /* Stop sampling */
    pio_sm_set_enabled(instance->pio, instance->sm, false);

    /* Stop DMA and count bytes already transferred */
    dma_channel_abort(instance->dma_rx_channel);
    size_t received =
        PIO_GET_FRAME_SIZE - dma_channel_hw_addr(instance->dma_rx_channel)->transfer_count;

    /* Drain any bytes still sitting in the PIO RX FIFO for an exact count */
    while(!pio_sm_is_rx_fifo_empty(instance->pio, instance->sm)) {
        if(received < PIO_GET_FRAME_SIZE) {
            instance->frame_buffers[instance->current_frame].data[received++] =
                (uint8_t)pio_sm_get(instance->pio, instance->sm);
        } else {
            pio_sm_get(instance->pio, instance->sm); /* discard overflow */
        }
    }

    if(instance->callback_rx && received > 0) {
        instance->callback_rx(
            instance->frame_buffers[instance->current_frame].data, received, instance->callback_context);
    }

    /* Switch to the next buffer and re-arm for the next frame */
    instance->current_frame = (instance->current_frame + 1) % PIO_GET_FRAME_COUNT;
    pio_sm_clear_fifos(instance->pio, instance->sm);
    pio_sm_restart(instance->pio, instance->sm); /* PC -> program start (wait CS low) */
    dma_channel_set_write_addr(instance->dma_rx_channel, instance->frame_buffers[instance->current_frame].data, false);
    dma_channel_set_trans_count(instance->dma_rx_channel, PIO_GET_FRAME_SIZE, false);
    dma_channel_start(instance->dma_rx_channel);
    pio_sm_set_enabled(instance->pio, instance->sm, true); /* waits for CS low */
}

PioGetFrame* pio_get_frame_init(void) {
    furi_check(pio_get_frame_instance == NULL); // Only one instance allowed
    PioGetFrame* instance = (PioGetFrame*)malloc(sizeof(PioGetFrame));
    furi_check(instance);
    pio_get_frame_instance = instance;
    instance->current_frame = 0;
    instance->callback_rx = NULL;
    instance->callback_context = NULL;

    /* "cpu spi" bus pins */
    const GpioPin* gpio_cs = &gpio_cpu_spi_cs;
    const GpioPin* gpio_sck = &gpio_cpu_spi_sck;
    const GpioPin* gpio_data = &gpio_cpu_spi_mosi;

    /* Claim a free state machine + add the program on a PIO covering the pins */
    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(
        &pio_get_frame_program,
        &instance->pio,
        &instance->sm,
        &instance->offset,
        gpio_cs->pin,
        gpio_data->pin,
        false);
    furi_check(success);

    /* Route CS/SCK/DATA to the PIO and configure as inputs */
    GpioAltFn alt_fn = pio_get_frame_pio_altfn(instance->pio);
    furi_hal_gpio_init_ex(gpio_cs, GpioModeInput, GpioPullUp, GpioSpeedLow, alt_fn);
    furi_hal_gpio_init_ex(gpio_sck, GpioModeInput, GpioPullUp, GpioSpeedLow, alt_fn);
    furi_hal_gpio_init_ex(gpio_data, GpioModeInput, GpioPullUp, GpioSpeedLow, alt_fn);

    /* State machine configuration */
    pio_sm_config c = pio_get_frame_program_get_default_config(instance->offset);
    sm_config_set_in_pins(&c, gpio_data->pin);
    sm_config_set_jmp_pin(&c, gpio_cs->pin);
    sm_config_set_in_shift(&c, false, true, 8); /* shift left, autopush 8 -> MSB-first byte */
    sm_config_set_clkdiv(&c, 1.0f);
    pio_sm_init(instance->pio, instance->sm, instance->offset, &c);
    pio_sm_set_enabled(instance->pio, instance->sm, false);

    /* Bypass input synchronizers on the timing-critical pins for max speed */
    instance->pio->input_sync_bypass =
        (1u << gpio_sck->pin) | (1u << gpio_cs->pin);

    /* DMA: PIO RX FIFO -> current frame buffer */
    instance->dma_rx_channel = dma_claim_unused_channel(true);
    furi_check(dma_channel_is_claimed(instance->dma_rx_channel));
    dma_channel_config dc = dma_channel_get_default_config(instance->dma_rx_channel);
    channel_config_set_transfer_data_size(&dc, DMA_SIZE_8);
    channel_config_set_read_increment(&dc, false);
    channel_config_set_write_increment(&dc, true);
    channel_config_set_dreq(&dc, pio_get_dreq(instance->pio, instance->sm, false));
    dma_channel_configure(
        instance->dma_rx_channel,
        &dc,
        instance->frame_buffers[instance->current_frame].data,
        &instance->pio->rxf[instance->sm],
        PIO_GET_FRAME_SIZE,
        false);

    /* CS rising edge = frame complete -> switch DMA buffer */
    furi_hal_gpio_add_int_callback(gpio_cs, GpioConditionRise, pio_get_frame_cs_isr, instance);

    /* Arm: SM waits for CS low, DMA ready */
    pio_sm_clear_fifos(instance->pio, instance->sm);
    pio_sm_restart(instance->pio, instance->sm);
    pio_sm_set_enabled(instance->pio, instance->sm, true);
    dma_channel_start(instance->dma_rx_channel);

    FURI_LOG_I(
        TAG,
        "PIO frame reader: PIO%d SM%d data=%d sck=%d cs=%d (frame %zu B x %u)",
        instance->pio == pio0 ? 0 : (instance->pio == pio1 ? 1 : 2),
        instance->sm,
        gpio_data->pin,
        gpio_sck->pin,
        gpio_cs->pin,
        (size_t)PIO_GET_FRAME_SIZE,
        PIO_GET_FRAME_COUNT);

    return instance;
}

void pio_get_frame_deinit(PioGetFrame* instance) {
    furi_check(instance);

    furi_hal_gpio_remove_int_callback(&gpio_cpu_spi_cs);
    pio_sm_set_enabled(instance->pio, instance->sm, false);
    dma_channel_unclaim(instance->dma_rx_channel);
    pio_remove_program_and_unclaim_sm(&pio_get_frame_program, instance->pio, instance->sm, instance->offset);

    /* Deinitialize GPIOs */
    furi_hal_gpio_init_ex(&gpio_cpu_spi_cs, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_init_ex(&gpio_cpu_spi_sck, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_init_ex(&gpio_cpu_spi_mosi, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);

    free(instance);
    pio_get_frame_instance = NULL;
}

void pio_get_frame_set_callback_rx(PioGetFrame* instance, PioGetFrameCallbackRx callback, void* context) {
    furi_check(instance);
    instance->callback_rx = callback;
    instance->callback_context = context;
}
