#include "pio_get_frame.h"

#include <furi.h>
#include <furi_hal_gpio.h>
#include <drivers/display/display_jd9853_reg.h>

#include <hardware/pio.h>
#include <hardware/pio_instructions.h>
#include <hardware/dma.h>

#define TAG "PioGetFrame"

#define PIO_GET_FRAME_SIZE  (JD9853_WIDTH * JD9853_HEIGHT)
#define PIO_GET_FRAME_COUNT 2

/* Runtime-assembled program layout (pins are passed via init, so the PIO
 * program is built at runtime with the actual GPIO numbers). SCK is sampled
 * on its rising edge (the capture edge for both SPI Mode 0 and Mode 3). The
 * pre-loop wait for the first falling edge removes the extra bogus sample a
 * Mode 3 (idle-high) clock would otherwise inject at frame start:
 *   0: wait 0 gpio <cs>     (frame start)
 *   1: wait 0 gpio <sck>    (sync to first falling edge, pre-loop)
 *   2: wait 1 gpio <sck>    <- loop start (wrap target)
 *   3: in pins, 1
 *   4: wait 0 gpio <sck>
 *   5: jmp pin frame_done   <- loop end (wrap)
 *   6: frame_done: jmp 6    (self-loop, parks until SM disabled)
 */
#define PIO_GET_FRAME_PROGRAM_LEN 7
#define PIO_GET_FRAME_LOOP_START  2
#define PIO_GET_FRAME_LOOP_END    5

typedef struct {
    uint8_t data[PIO_GET_FRAME_SIZE];
} PioGetFrameBuffer;

struct PioGetFrame {
    const GpioPin* gpio_cs;
    const GpioPin* gpio_sck;
    const GpioPin* gpio_data;
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

static void __not_in_flash_func(pio_get_frame_rearm)(PioGetFrame* instance) {
    instance->current_frame = (instance->current_frame + 1) % PIO_GET_FRAME_COUNT;
    pio_sm_set_enabled(instance->pio, instance->sm, false);

    pio_sm_clear_fifos(instance->pio, instance->sm);
    pio_sm_restart(instance->pio, instance->sm);

    /* Force the PC to the program start (pio_sm_restart clears the PC to 0,
     * which is only correct when the program happens to sit at offset 0). */
    pio_sm_exec(instance->pio, instance->sm, pio_encode_jmp(instance->offset));
    dma_channel_set_write_addr(instance->dma_rx_channel, instance->frame_buffers[instance->current_frame].data, false);
    dma_channel_set_trans_count(instance->dma_rx_channel, PIO_GET_FRAME_SIZE, false);
    dma_channel_start(instance->dma_rx_channel);
    pio_sm_set_enabled(instance->pio, instance->sm, true); /* waits for CS low */
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
    size_t received = PIO_GET_FRAME_SIZE - dma_channel_hw_addr(instance->dma_rx_channel)->transfer_count;

    /* Drain any bytes still sitting in the PIO RX FIFO for an exact count */
    while(!pio_sm_is_rx_fifo_empty(instance->pio, instance->sm)) {
        if(received < PIO_GET_FRAME_SIZE) {
            instance->frame_buffers[instance->current_frame].data[received++] = (uint8_t)pio_sm_get(instance->pio, instance->sm);
        } else {
            pio_sm_get(instance->pio, instance->sm); /* discard overflow */
        }
    }

    const bool complete = (received == PIO_GET_FRAME_SIZE);

    if(instance->callback_rx && complete) {
        instance->callback_rx(instance->frame_buffers[instance->current_frame].data, received, instance->callback_context);
    }

    /* Switch to the next buffer and re-arm for the next frame */
    pio_get_frame_rearm(instance);
}

PioGetFrame* pio_get_frame_init(const GpioPin* gpio_cs, const GpioPin* gpio_sck, const GpioPin* gpio_data) {
    furi_check(pio_get_frame_instance == NULL); // Only one instance allowed
    PioGetFrame* instance = (PioGetFrame*)malloc(sizeof(PioGetFrame));
    pio_get_frame_instance = instance;
    instance->gpio_cs = gpio_cs;
    instance->gpio_sck = gpio_sck;
    instance->gpio_data = gpio_data;
    instance->current_frame = 0;
    instance->callback_rx = NULL;
    instance->callback_context = NULL;

    /* Build the program at runtime with the actual GPIO numbers. The bus pins
     * are not contiguous, so a static .pio cannot be pin-agnostic; the SDK
     * relocates the jmp target automatically when the program is installed. */
    uint16_t instructions[PIO_GET_FRAME_PROGRAM_LEN];
    instructions[0] = pio_encode_wait_gpio(false, gpio_cs->pin);
    instructions[1] = pio_encode_wait_gpio(false, gpio_sck->pin);
    instructions[2] = pio_encode_wait_gpio(true, gpio_sck->pin);
    instructions[3] = pio_encode_in(pio_pins, 1);
    instructions[4] = pio_encode_wait_gpio(false, gpio_sck->pin);
    instructions[5] = pio_encode_jmp_pin(6); /* -> frame_done */
    instructions[6] = pio_encode_jmp(6); /* frame_done: self-loop, parks until SM disabled */

    const pio_program_t program = {
        .instructions = instructions,
        .length = PIO_GET_FRAME_PROGRAM_LEN,
        .origin = -1,
    };

    /* Claim a free state machine + add the program on a PIO covering the pins */
    /* Arguments are (gpio_base, gpio_count): lowest pin and the size of the
     * pin range (25..28 -> base 25, count 4). Do not change the GPIO base: */
    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(
        &program, &instance->pio, &instance->sm, &instance->offset, gpio_cs->pin, gpio_data->pin - gpio_cs->pin + 1, false);
    furi_check(success);

    /* Route CS/SCK/DATA to the PIO and configure as inputs */
    GpioAltFn alt_fn = pio_get_frame_pio_altfn(instance->pio);
    furi_hal_gpio_init_ex(gpio_cs, GpioModeInput, GpioPullUp, GpioSpeedLow, alt_fn);
    furi_hal_gpio_init_ex(gpio_sck, GpioModeInput, GpioPullUp, GpioSpeedLow, alt_fn);
    furi_hal_gpio_init_ex(gpio_data, GpioModeInput, GpioPullUp, GpioSpeedLow, alt_fn);

    /* State machine configuration */
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, instance->offset + PIO_GET_FRAME_LOOP_START, instance->offset + PIO_GET_FRAME_LOOP_END);
    sm_config_set_in_pins(&c, gpio_data->pin);
    sm_config_set_jmp_pin(&c, gpio_cs->pin);
    sm_config_set_in_pin_count(&c, 32);
    sm_config_set_in_shift(&c, false, true, 8); /* shift left, autopush 8 -> MSB-first byte */
    sm_config_set_clkdiv(&c, 1.0f);
    /* TX FIFO is unused (no OUT instructions): join it to RX for a free 8-word
     * deep RX FIFO instead of 4, giving DMA more slack before an RXSTALL if the
     * bus/DMA is briefly busy elsewhere. Pure hardware config, zero CPU cost. */
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
    pio_sm_init(instance->pio, instance->sm, instance->offset, &c);
    pio_sm_set_enabled(instance->pio, instance->sm, false);

    /* INPUT_SYNC_BYPASS bits are relative to the PIO's GPIO base. On RP2350B
     * PIO1's base is 16 (set by the debug UART on pin 41), so absolute pins
     * would address the wrong GPIOs. Bypass SCK, CS and DATA all together so
     * the fast external clock is not delayed by the 2-cycle synchronizers and
     * all three inputs stay aligned with zero skew.
     * Use read-modify-write so other drivers on the same PIO (e.g. i2c) keep
     * their own bypass configuration. */
    const uint gpio_base = pio_get_gpio_base(instance->pio);
    instance->pio->input_sync_bypass |= (1u << (gpio_sck->pin - gpio_base)) | (1u << (gpio_cs->pin - gpio_base)) | (1u << (gpio_data->pin - gpio_base));

    /* DMA: PIO RX FIFO -> current frame buffer */
    instance->dma_rx_channel = dma_claim_unused_channel(true);
    furi_check(dma_channel_is_claimed(instance->dma_rx_channel));
    dma_channel_config dc = dma_channel_get_default_config(instance->dma_rx_channel);
    channel_config_set_transfer_data_size(&dc, DMA_SIZE_8);
    channel_config_set_read_increment(&dc, false);
    channel_config_set_write_increment(&dc, true);
    channel_config_set_dreq(&dc, pio_get_dreq(instance->pio, instance->sm, false));
    /* High DMA bus-arbitration priority: this channel must never lose a bus
     * cycle to another DMA channel (e.g. the display TX DMA). The PIO's RX
     * FIFO is only 8 entries deep (after RX/TX join) at full bus-clock rate,
     * so any arbitration delay risks an RXSTALL and dropped samples. Pure
     * register configuration, zero recurring CPU cost. */
    channel_config_set_high_priority(&dc, true);
    dma_channel_configure(
        instance->dma_rx_channel, &dc, instance->frame_buffers[instance->current_frame].data, &instance->pio->rxf[instance->sm], PIO_GET_FRAME_SIZE, false);

    /* CS rising edge = frame complete -> switch DMA buffer */
    furi_hal_gpio_add_int_callback(gpio_cs, GpioConditionRise, pio_get_frame_cs_isr, instance);

    /* Arm: SM waits for CS low, DMA ready */
    pio_sm_clear_fifos(instance->pio, instance->sm);
    pio_sm_restart(instance->pio, instance->sm);
    pio_sm_set_enabled(instance->pio, instance->sm, true);
    dma_channel_start(instance->dma_rx_channel);

    FURI_LOG_D(
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

    furi_hal_gpio_remove_int_callback(instance->gpio_cs);
    pio_sm_set_enabled(instance->pio, instance->sm, false);

    /* Abort DMA before unclaim — the channel may still be armed (EN=1) even
     * though the SM is stopped. Without abort, the channel stays enabled in
     * hardware and continues to burn an arbitration slot on the bus. */
    dma_channel_abort(instance->dma_rx_channel);
    dma_channel_unclaim(instance->dma_rx_channel);

    /* Only .length is used to free the instruction space */
    const pio_program_t program = {
        .instructions = NULL,
        .length = PIO_GET_FRAME_PROGRAM_LEN,
        .origin = -1,
    };
    pio_remove_program_and_unclaim_sm(&program, instance->pio, instance->sm, instance->offset);

    /* Clear input_sync_bypass for the pins we owned, so that any other driver
     * (e.g. i2c_master_pio) on the same PIO block does not inherit bypassed
     * synchronizers on unrelated pins. Without this, a subsequent driver sees
     * leftover bypass bits from this driver's init, which can corrupt I2C
     * SDA/SCL sampling. */
    const uint gpio_base = pio_get_gpio_base(instance->pio);
    instance->pio->input_sync_bypass &=
        ~((1u << (instance->gpio_sck->pin - gpio_base)) | (1u << (instance->gpio_cs->pin - gpio_base)) | (1u << (instance->gpio_data->pin - gpio_base)));

    /* Deinitialize GPIOs */
    furi_hal_gpio_init_ex(instance->gpio_cs, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_init_ex(instance->gpio_sck, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_init_ex(instance->gpio_data, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);

    free(instance);
    pio_get_frame_instance = NULL;
}

void pio_get_frame_set_callback_rx(PioGetFrame* instance, PioGetFrameCallbackRx callback, void* context) {
    furi_check(instance);
    instance->callback_rx = callback;
    instance->callback_context = context;
}
