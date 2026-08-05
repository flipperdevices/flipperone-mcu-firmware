#include "pio_get_frame.h"

#include <furi.h>
#include <furi_hal_gpio.h>
#include <drivers/display/display_jd9853_reg.h>

#include <hardware/pio.h>
#include <hardware/pio_instructions.h>
#include <hardware/dma.h>

#define TAG "PioGetFrame"

#define PIO_GET_FRAME_SIZE       (JD9853_WIDTH * JD9853_HEIGHT)
#define PIO_GET_FRAME_COUNT      2

/* Self-heal: if the receiver gets stuck producing all-zero frames while the
 * DATA line is active (cold start), force a full re-arm to recover. Define to
 * enable; leave undefined to compile the self-heal logic out entirely. */
//#define PIO_GET_FRAME_SELF_HEAL

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
#ifdef PIO_GET_FRAME_SELF_HEAL
    dma_channel_config dma_config;    /* saved for a full receiver re-arm */
    uint32_t consecutive_zero_frames; /* consecutive all-zero frames */
    bool has_good_frame;              /* true once a non-zero frame was received */
    uint32_t full_resets_done;        /* total full re-arms performed (limit) */
#endif
    PioGetFrameCallbackRx callback_rx;
    void* callback_context;
};

static PioGetFrame* pio_get_frame_instance = NULL;

static GpioAltFn pio_get_frame_pio_altfn(PIO pio) {
    if(pio == pio0) return GpioAltFn6Pio0;
    if(pio == pio1) return GpioAltFn7Pio1;
    return GpioAltFn8Pio2;
}

/* Re-arm the SM + DMA for the next frame. With the self-heal enabled,
 * full_reset additionally re-applies the saved DMA config, restarts the SM
 * clock divider and forces the PC to the program start — used to recover when
 * a bad first frame from the master leaves the receiver stuck producing
 * all-zero buffers (cold start). */
static void __not_in_flash_func(pio_get_frame_rearm)(PioGetFrame* instance
#ifdef PIO_GET_FRAME_SELF_HEAL
    , bool full_reset
#endif
) {
    instance->current_frame = (instance->current_frame + 1) % PIO_GET_FRAME_COUNT;
    pio_sm_set_enabled(instance->pio, instance->sm, false);
#ifdef PIO_GET_FRAME_SELF_HEAL
    if(full_reset) {
        dma_channel_abort(instance->dma_rx_channel);
        pio_sm_clear_fifos(instance->pio, instance->sm);
        pio_sm_restart(instance->pio, instance->sm);
        pio_sm_clkdiv_restart(instance->pio, instance->sm);
        dma_channel_set_config(instance->dma_rx_channel, &instance->dma_config, false);
    } else
#endif
    {
        pio_sm_clear_fifos(instance->pio, instance->sm);
        pio_sm_restart(instance->pio, instance->sm);
    }
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

    const bool complete = (received == PIO_GET_FRAME_SIZE);

    /* Deliver only complete, aligned frames. A partial frame (received <
     * PIO_GET_FRAME_SIZE) means capture started mid-frame (e.g. the driver was
     * brought up while a frame was already in flight) or a CS glitch — drop it
     * and rely on the next CS-low for a clean frame boundary. */
#ifdef PIO_GET_FRAME_SELF_HEAL
    uint32_t frame_sum = 0;
    /* The checksum is needed only by the self-heal, and only until the first
     * real (non-zero) frame arrives. Skipping it afterwards keeps the ISR on
     * the fast path (no 37 KB scan per frame in steady state). */
    if(complete && !instance->has_good_frame) {
        const uint8_t* data = instance->frame_buffers[instance->current_frame].data;
        for(size_t i = 0; i < PIO_GET_FRAME_SIZE; i++) {
            frame_sum += data[i];
        }
    }
#endif
    if(instance->callback_rx && complete) {
        instance->callback_rx(
            instance->frame_buffers[instance->current_frame].data, received, instance->callback_context);
    }

#ifdef PIO_GET_FRAME_SELF_HEAL
    /* Self-heal, gated so it never churns on legitimate all-black content:
     *  - only all-zero frames,
     *  - only while the DATA line is actively driven HIGH (receiver stuck),
     *  - only during the cold-start phase (no good frame received yet),
     *  - and only up to a few attempts. A master that really sends black holds
     *    the line LOW, so data_high==false -> no re-arm, black passes through. */
    bool need_full_reset = false;
    if(complete && !instance->has_good_frame) {
        const bool data_high = (gpio_get(instance->gpio_data->pin) == 1);
        if(frame_sum == 0) {
            instance->consecutive_zero_frames++;
            need_full_reset =
                data_high && (instance->consecutive_zero_frames >= 3) &&
                (instance->full_resets_done < 3);
            if(need_full_reset) {
                instance->full_resets_done++;
                FURI_LOG_W(
                    TAG,
                    "Self-heal: %lu all-zero frames on an active DATA line, full re-arm (%lu/%lu)",
                    (unsigned long)instance->consecutive_zero_frames,
                    (unsigned long)instance->full_resets_done,
                    3UL);
            }
        } else {
            instance->consecutive_zero_frames = 0;
            instance->has_good_frame = true;
        }
    }
#endif

    /* Switch to the next buffer and re-arm for the next frame */
#ifdef PIO_GET_FRAME_SELF_HEAL
    pio_get_frame_rearm(instance, need_full_reset);
#else
    pio_get_frame_rearm(instance);
#endif
}

PioGetFrame* pio_get_frame_init(const GpioPin* gpio_cs, const GpioPin* gpio_sck, const GpioPin* gpio_data) {
    furi_check(pio_get_frame_instance == NULL); // Only one instance allowed
    PioGetFrame* instance = (PioGetFrame*)malloc(sizeof(PioGetFrame));
    furi_check(instance);
    pio_get_frame_instance = instance;
    instance->gpio_cs = gpio_cs;
    instance->gpio_sck = gpio_sck;
    instance->gpio_data = gpio_data;
    instance->current_frame = 0;
#ifdef PIO_GET_FRAME_SELF_HEAL
    instance->consecutive_zero_frames = 0;
    instance->has_good_frame = false;
    instance->full_resets_done = 0;
#endif
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
    instructions[6] = pio_encode_jmp(6);     /* frame_done: self-loop, parks until SM disabled */

    const pio_program_t program = {
        .instructions = instructions,
        .length = PIO_GET_FRAME_PROGRAM_LEN,
        .origin = -1,
    };

    /* Claim a free state machine + add the program on a PIO covering the pins */
    /* Arguments are (gpio_base, gpio_count): lowest pin and the size of the
     * pin range (25..28 -> base 25, count 4). Do not change the GPIO base:
     * PIO1 is shared with the debug UART which requires base 16 for pin 41. */
    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(
        &program,
        &instance->pio,
        &instance->sm,
        &instance->offset,
        gpio_cs->pin,
        gpio_data->pin - gpio_cs->pin + 1,
        false);
    furi_check(success);

    /* Route CS/SCK/DATA to the PIO and configure as inputs */
    GpioAltFn alt_fn = pio_get_frame_pio_altfn(instance->pio);
    furi_hal_gpio_init_ex(gpio_cs, GpioModeInput, GpioPullUp, GpioSpeedLow, alt_fn);
    furi_hal_gpio_init_ex(gpio_sck, GpioModeInput, GpioPullUp, GpioSpeedLow, alt_fn);
    furi_hal_gpio_init_ex(gpio_data, GpioModeInput, GpioPullUp, GpioSpeedLow, alt_fn);

    /* State machine configuration */
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(
        &c,
        instance->offset + PIO_GET_FRAME_LOOP_START,
        instance->offset + PIO_GET_FRAME_LOOP_END);
    sm_config_set_in_pins(&c, gpio_data->pin);
    sm_config_set_jmp_pin(&c, gpio_cs->pin);
    sm_config_set_in_pin_count(&c, 32);
    sm_config_set_in_shift(&c, false, true, 8); /* shift left, autopush 8 -> MSB-first byte */
    sm_config_set_clkdiv(&c, 1.0f);
    pio_sm_init(instance->pio, instance->sm, instance->offset, &c);
    pio_sm_set_enabled(instance->pio, instance->sm, false);

    /* INPUT_SYNC_BYPASS bits are relative to the PIO's GPIO base. On RP2350B
     * PIO1's base is 16 (set by the debug UART on pin 41), so absolute pins
     * would address the wrong GPIOs. Bypass SCK, CS and DATA all together so
     * the fast external clock is not delayed by the 2-cycle synchronizers and
     * all three inputs stay aligned with zero skew. */
    const uint gpio_base = pio_get_gpio_base(instance->pio);
    instance->pio->input_sync_bypass =
        (1u << (gpio_sck->pin - gpio_base)) |
        (1u << (gpio_cs->pin - gpio_base)) |
        (1u << (gpio_data->pin - gpio_base));

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
#ifdef PIO_GET_FRAME_SELF_HEAL
    instance->dma_config = dc; /* save for a full re-arm on self-heal */
#endif

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

    furi_hal_gpio_remove_int_callback(instance->gpio_cs);
    pio_sm_set_enabled(instance->pio, instance->sm, false);
    dma_channel_unclaim(instance->dma_rx_channel);

    /* Only .length is used to free the instruction space */
    const pio_program_t program = {
        .instructions = NULL,
        .length = PIO_GET_FRAME_PROGRAM_LEN,
        .origin = -1,
    };
    pio_remove_program_and_unclaim_sm(&program, instance->pio, instance->sm, instance->offset);

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
