#include "display_jd9853_qspi.h"
#include "display_jd9853_reg.h"

#include <furi.h>
#include <furi_hal_gpio.h>
#include <furi_hal_resources.h>

#include <hardware/structs/clocks.h>
#include <hardware/structs/hstx_ctrl.h>
#include <hardware/structs/hstx_fifo.h>
#include <pico/time.h>
#include <pico/types.h>
#include <hardware/dma.h>

#include <hardware/clocks.h>

#define TAG "DisplayDriver"

#define FIRST_HSTX_PIN                             (12u)
#define DISPLAY_JD9853_HSTX_END_TX_DELAY_US        (5u) //5us
#define DISPLAY_JD9853_CONNECTION_CHECK_TIMEOUT_MS (1000u)
#define DISPLAY_JD9853_DEINIT_TIMEOUT_MS           (100u)
#define DISPLAY_JD9853_RESET_DELAY_MS              (150u)

//#define DISPLAY_JD9853_SHOW_FPS
#define DISPLAY_JD9853_DYNAMIC_REFRESH

#ifdef DISPLAY_JD9853_DYNAMIC_REFRESH
#define DISPLAY_JD9853_REFRESH_CHECK_INTERVAL_MS (2000u) // 2 s
#define DISPLAY_JD9853_REFRESH_FAST_FRAMES       (4u) // consecutive fast frames needed to step up
#define DISPLAY_JD9853_REFRESH_HOLD_MS           (5000u) // no step down this long after a step up
#define DISPLAY_JD9853_REFRESH_IDLE_US           (2000000u) // a frame after this much idle jumps to max
#endif

typedef struct {
    uint32_t cmd[4];
    uint8_t data[JD9853_WIDTH * JD9853_HEIGHT];
} DisplayJd9853QSPIBufferHeader;

struct DisplayJd9853QSPI {
    FuriSemaphore* busy;
    uint32_t dma_tx_channel;
    DisplayJd9853QSPIBufferHeader buffer_header;
    bool display_is_connected;
#ifdef DISPLAY_JD9853_DYNAMIC_REFRESH
    volatile DisplayJd9853RefreshStep refresh_step;
    volatile DisplayJd9853RefreshStep refresh_pending;
    volatile uint32_t frames_in_window;
    volatile bool refresh_backpressure;
    volatile uint32_t refresh_hold_until;
    uint32_t last_frame_us;
    uint32_t fast_frames;
    FuriTimer* refresh_timer;
#endif
#ifdef DISPLAY_JD9853_SHOW_FPS
    volatile uint32_t frames_written;
    FuriTimer* fps_timer;
#endif
};

static DisplayJd9853QSPI* display_instance = NULL;

#ifdef DISPLAY_JD9853_SHOW_FPS
static void display_jd9853_fps_timer_callback(void* ctx) {
    UNUSED(ctx);
    DisplayJd9853QSPI* display = display_instance;
    if(!display) return;
    uint32_t fps = display->frames_written;
    display->frames_written = 0;

#ifdef DISPLAY_JD9853_DYNAMIC_REFRESH
    FURI_LOG_W(
        TAG,
        "Display TX: %lu FPS, panel %u FPS (step %u)",
        (unsigned long)fps,
        (unsigned)jd9853_refresh_config[display->refresh_step].fps,
        (unsigned)display->refresh_step);
#else
    FURI_LOG_W(TAG, "Display TX: %lu FPS", (unsigned long)fps);
#endif
}
#endif

static FURI_ALWAYS_INLINE void display_jd9853_hstx_wait_complete(DisplayJd9853QSPI* display) {
    UNUSED(display);
    while(!(hstx_fifo_hw->stat & HSTX_FIFO_STAT_EMPTY_BITS)) {
        furi_thread_yield();
    }
}

static FURI_ALWAYS_INLINE void display_jd9853_hstx_config_1_line(void) {
    hstx_ctrl_hw->bit[gpio_display_scl.pin - FIRST_HSTX_PIN] = HSTX_CTRL_BIT0_CLK_BITS;

    hstx_ctrl_hw->bit[gpio_display_sda.pin - FIRST_HSTX_PIN] = (7u << HSTX_CTRL_BIT0_SEL_P_LSB) | (7u << HSTX_CTRL_BIT0_SEL_N_LSB);
    hstx_ctrl_hw->bit[gpio_display_d0.pin - FIRST_HSTX_PIN] = (7u << HSTX_CTRL_BIT0_SEL_P_LSB) | (7u << HSTX_CTRL_BIT0_SEL_N_LSB);

    hstx_ctrl_hw->bit[gpio_display_d1.pin - FIRST_HSTX_PIN] = (7u << HSTX_CTRL_BIT0_SEL_P_LSB) | (7u << HSTX_CTRL_BIT0_SEL_N_LSB);
    hstx_ctrl_hw->bit[gpio_display_d2.pin - FIRST_HSTX_PIN] = (7u << HSTX_CTRL_BIT0_SEL_P_LSB) | (7u << HSTX_CTRL_BIT0_SEL_N_LSB);

    hstx_ctrl_hw->bit[gpio_display_cs.pin - FIRST_HSTX_PIN] = (27u << HSTX_CTRL_BIT0_SEL_P_LSB) | (27u << HSTX_CTRL_BIT0_SEL_N_LSB);

    //We have packed 8-bit fields, so shift left 1 bit/cycle, 8 times.
    hstx_ctrl_hw->csr = HSTX_CTRL_CSR_EN_BITS | (31u << HSTX_CTRL_CSR_SHIFT_LSB) | (8u << HSTX_CTRL_CSR_N_SHIFTS_LSB) | (1u << HSTX_CTRL_CSR_CLKDIV_LSB);

    furi_hal_gpio_set_function(&gpio_display_cs, GpioAltFn0Hstx);
}

static FURI_ALWAYS_INLINE void display_jd9853_hstx_init_1_line(DisplayJd9853QSPI* display) {
    display_jd9853_hstx_wait_complete(display);
    display_jd9853_hstx_config_1_line();
}

static FURI_ALWAYS_INLINE void display_jd9853_hstx_config_4_line(void) {
    hstx_ctrl_hw->bit[gpio_display_scl.pin - FIRST_HSTX_PIN] = HSTX_CTRL_BIT0_CLK_BITS;

    hstx_ctrl_hw->bit[gpio_display_sda.pin - FIRST_HSTX_PIN] = (28u << HSTX_CTRL_BIT0_SEL_P_LSB) | (28u << HSTX_CTRL_BIT0_SEL_N_LSB);
    hstx_ctrl_hw->bit[gpio_display_d0.pin - FIRST_HSTX_PIN] = (29u << HSTX_CTRL_BIT0_SEL_P_LSB) | (29u << HSTX_CTRL_BIT0_SEL_N_LSB);

    hstx_ctrl_hw->bit[gpio_display_d1.pin - FIRST_HSTX_PIN] = (30u << HSTX_CTRL_BIT0_SEL_P_LSB) | (30u << HSTX_CTRL_BIT0_SEL_N_LSB);
    hstx_ctrl_hw->bit[gpio_display_d2.pin - FIRST_HSTX_PIN] = (31u << HSTX_CTRL_BIT0_SEL_P_LSB) | (31u << HSTX_CTRL_BIT0_SEL_N_LSB);

    hstx_ctrl_hw->bit[gpio_display_cs.pin - FIRST_HSTX_PIN] = 0x00;
    //We have packed 32-bit fields, so shift left 4 bit/cycle, 8 times.
    hstx_ctrl_hw->csr = HSTX_CTRL_CSR_EN_BITS | (28u << HSTX_CTRL_CSR_SHIFT_LSB) | (8u << HSTX_CTRL_CSR_N_SHIFTS_LSB) | (1u << HSTX_CTRL_CSR_CLKDIV_LSB);

    furi_hal_gpio_set_function(&gpio_display_cs, GpioAltFn5Sio);
}

static FURI_ALWAYS_INLINE void display_jd9853_hstx_init_4_line(DisplayJd9853QSPI* display) {
    display_jd9853_hstx_wait_complete(display);
    display_jd9853_hstx_config_4_line();
}

static FURI_ALWAYS_INLINE void display_jd9853_hstx_put_word(uint32_t data) {
    while(hstx_fifo_hw->stat & HSTX_FIFO_STAT_FULL_BITS) {
        furi_thread_yield();
    }
    hstx_fifo_hw->fifo = data;
}

static FURI_ALWAYS_INLINE void display_jd9853_dma_put_buffer(DisplayJd9853QSPI* display, const uint8_t* data, size_t size) {
    display_jd9853_hstx_wait_complete(display);

    dma_channel_set_read_addr(display->dma_tx_channel, data, false);
    dma_channel_set_transfer_count(display->dma_tx_channel, size / 4, false);

    // Start DMA transfer
    dma_channel_start(display->dma_tx_channel);
}

static FURI_ALWAYS_INLINE void display_jd9853_cs_up(void) {
    display_jd9853_hstx_put_word(0x0ff00000u);
}

static FURI_ALWAYS_INLINE void display_jd9853_write_reg_1line(DisplayJd9853QSPI* display, DisplayJd9853Reg reg) {
    UNUSED(display);
    display_jd9853_hstx_put_word(JD9853_QSPI_CMD_1_LINE_MODE); // Command Write Quad SPI
    display_jd9853_hstx_put_word((uint8_t)0x00);
    display_jd9853_hstx_put_word((uint8_t)reg);
    display_jd9853_hstx_put_word((uint8_t)0x00);
}

uint32_t convert_to_dual_line_compact(uint8_t data, uint8_t line) {
    uint32_t result = 0;

    for(size_t i = 0; i < 8; i++) {
        result <<= line;
        if(data & 0x80) {
            result |= 0x1;
        }
        data <<= 1;
    }
    return result;
}

static FURI_ALWAYS_INLINE void display_jd9853_write_reg(DisplayJd9853QSPI* display, DisplayJd9853Reg reg) {
    display_jd9853_cs_up();
    display_jd9853_write_reg_1line(display, reg);
}

static FURI_ALWAYS_INLINE void display_jd9853_write_data(DisplayJd9853QSPI* display, uint8_t* data, size_t size) {
    UNUSED(display);
    for(size_t i = 0; i < size; i++) {
        display_jd9853_hstx_put_word(data[i]);
    }
    display_jd9853_cs_up();
}

#ifdef DISPLAY_JD9853_DYNAMIC_REFRESH
static void display_jd9853_apply_refresh(DisplayJd9853QSPI* display) {
    DisplayJd9853RefreshStep pending;
    {
        FURI_CRITICAL_ENTER();
        pending = display->refresh_pending;
        display->refresh_pending = DisplayJd9853RefreshNone;
        FURI_CRITICAL_EXIT();
    }

    if(pending == DisplayJd9853RefreshNone || pending >= DisplayJd9853RefreshCount || pending == display->refresh_step) {
        return;
    }

    display_jd9853_load_config(display, jd9853_refresh_config[pending].config);

    display->refresh_step = pending;
}

static void display_jd9853_refresh_timer_callback(void* ctx) {
    DisplayJd9853QSPI* display = ctx;

    if(display_instance != display) {
        return;
    }

    uint32_t frames;
    bool backpressure;
    bool hold;
    {
        FURI_CRITICAL_ENTER();
        frames = display->frames_in_window;
        backpressure = display->refresh_backpressure;
        hold = (int32_t)(furi_get_tick() - display->refresh_hold_until) < 0;
        display->frames_in_window = 0;
        display->refresh_backpressure = false;
        FURI_CRITICAL_EXIT();
    }

    DisplayJd9853RefreshStep current = display->refresh_step;
    if(!backpressure && !hold && (current != DisplayJd9853Refresh10)) {
        uint32_t incoming_fps = frames * 1000u / DISPLAY_JD9853_REFRESH_CHECK_INTERVAL_MS;
        DisplayJd9853RefreshStep next = current - 1;
        uint32_t next_fps = jd9853_refresh_config[next].fps;

        /* Require 20% headroom above the measured incoming frame rate. */
        if((next_fps * 100u) >= (incoming_fps * 120u)) {
            bool requested = false;
            {
                FURI_CRITICAL_ENTER();
                if(display->refresh_pending == DisplayJd9853RefreshNone) {
                    display->refresh_pending = next;
                    requested = true;
                }
                FURI_CRITICAL_EXIT();
            }

            if(requested && furi_semaphore_acquire(display->busy, 0) == FuriStatusOk) {
                display_jd9853_apply_refresh(display);
                furi_semaphore_release(display->busy);
            }
        }
    }

    if(display->refresh_step != DisplayJd9853Refresh10) {
        furi_timer_start(display->refresh_timer, furi_ms_to_ticks(DISPLAY_JD9853_REFRESH_CHECK_INTERVAL_MS));
    }
}
#endif

void display_jd9853_load_config(DisplayJd9853QSPI* display, const uint8_t* config) {
    display_jd9853_hstx_init_1_line(display);
    while(*config) {
        display_jd9853_write_reg(display, (DisplayJd9853Reg)(*(config + 2)));

        if(*(config)) {
            display_jd9853_write_data(display, (uint8_t*)(config + 3), *(config)-1);
        }
        furi_delay_ms(*(config + 1) * 5);
        config += *(config) + 2;
    }
    display_jd9853_hstx_init_4_line(display);
}

static void display_jd9853_enable(DisplayJd9853QSPI* display, bool enable) {
    furi_check(furi_semaphore_acquire(display->busy, FuriWaitForever) == FuriStatusOk);
    display_jd9853_hstx_init_1_line(display);

    display_jd9853_write_reg(display, enable ? dispon : dispoff);

    display_jd9853_hstx_init_4_line(display);
    furi_semaphore_release(display->busy);
}

static FURI_ALWAYS_INLINE void display_jd9853_set_window(DisplayJd9853QSPI* display, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t caset_data[4] = {(uint8_t)(x0 >> 8), (uint8_t)(x0 & 0xFF), (uint8_t)(x1 >> 8), (uint8_t)(x1 & 0xFF)};
    uint8_t paset_data[4] = {(uint8_t)(y0 >> 8), (uint8_t)(y0 & 0xFF), (uint8_t)(y1 >> 8), (uint8_t)(y1 & 0xFF)};

    furi_check(furi_semaphore_acquire(display->busy, FuriWaitForever) == FuriStatusOk);
    display_jd9853_hstx_init_1_line(display);

    display_jd9853_write_reg(display, caset); // Column address set
    display_jd9853_write_data(display, caset_data, sizeof(caset_data));

    display_jd9853_write_reg(display, paset); // Page address set
    display_jd9853_write_data(display, paset_data, sizeof(paset_data));

    display_jd9853_hstx_init_4_line(display);
    furi_semaphore_release(display->busy);
}

FURI_ALWAYS_INLINE void display_jd9853_qspi_write_buffer(DisplayJd9853QSPI* display, const uint8_t* buffer, size_t size) {
    furi_assert(display);
    furi_check(size == JD9853_WIDTH * JD9853_HEIGHT); //size must be equal to full buffer size

    if(!display->display_is_connected) {
        return; // Don't attempt to write if display is not connected
    }
#ifdef DISPLAY_JD9853_SHOW_FPS
    display->frames_written++;
#endif

#ifdef DISPLAY_JD9853_DYNAMIC_REFRESH
    uint32_t now_us = time_us_32();
    uint32_t delta_us = now_us - display->last_frame_us;
    display->last_frame_us = now_us;

    bool panel_busy = (furi_semaphore_get_count(display->busy) == 0);

    bool jump_to_max = false;
    bool step_up = false;
    if(delta_us >= DISPLAY_JD9853_REFRESH_IDLE_US) {
        jump_to_max = true;
        display->fast_frames = 0;
    } else if(panel_busy || (delta_us < jd9853_refresh_config[display->refresh_step].fast_delta_us)) {
        if(++display->fast_frames >= DISPLAY_JD9853_REFRESH_FAST_FRAMES) {
            step_up = true;
            display->fast_frames = 0;
        }
    } else {
        display->fast_frames = 0;
    }

    bool stepped_up = false;
    {
        FURI_CRITICAL_ENTER();
        display->frames_in_window++;
        if(panel_busy) {
            display->refresh_backpressure = true;
        }

        if(jump_to_max || step_up) {
            DisplayJd9853RefreshStep step = display->refresh_step;
            DisplayJd9853RefreshStep base = MAX(step, display->refresh_pending);
            DisplayJd9853RefreshStep target = jump_to_max ? DisplayJd9853RefreshMax : MIN(base + 1, DisplayJd9853RefreshMax);
            display->refresh_pending = (target > step) ? target : DisplayJd9853RefreshNone;
            display->refresh_hold_until = furi_get_tick() + furi_ms_to_ticks(DISPLAY_JD9853_REFRESH_HOLD_MS);
            stepped_up = true;
        }
        FURI_CRITICAL_EXIT();
    }

    if(stepped_up && display->refresh_timer) {
        furi_timer_start(display->refresh_timer, furi_ms_to_ticks(DISPLAY_JD9853_REFRESH_CHECK_INTERVAL_MS));
    }
#endif

    furi_check(furi_semaphore_acquire(display->busy, FuriWaitForever) == FuriStatusOk);

#ifdef DISPLAY_JD9853_DYNAMIC_REFRESH
    if(display->refresh_pending != DisplayJd9853RefreshNone) {
        display_jd9853_apply_refresh(display);
    }
#endif

    memcpy(display->buffer_header.data, buffer, size);

    furi_hal_gpio_enable_int_callback(&gpio_display_te);
}

void display_jd9853_qspi_fill(DisplayJd9853QSPI* display, uint8_t color) {
    furi_assert(display);
    const size_t width = JD9853_WIDTH; // 1 byte per pixel
    const size_t height = JD9853_HEIGHT;

    uint8_t* data = (uint8_t*)malloc(width * height);
    for(size_t i = 0; i < width * height; i += 1) {
        data[i] = color;
    }

    display_jd9853_qspi_write_buffer(display, data, width * height);
    free(data);
}

static void display_jd9853_qspi_clear(DisplayJd9853QSPI* display) {
    furi_assert(display);
    if(!display->display_is_connected) {
        return; // Don't attempt to write if display is not connected
    }

    memset(display->buffer_header.data, 0xFF, JD9853_WIDTH * JD9853_HEIGHT);

    furi_check(furi_semaphore_acquire(display->busy, FuriWaitForever) == FuriStatusOk);

    // Don't wait for TE, send the buffer instantly
    furi_hal_gpio_write(&gpio_display_cs, false);
    display_jd9853_dma_put_buffer(display, (uint8_t*)&display->buffer_header, sizeof(display->buffer_header));

    while(furi_semaphore_get_space(display->busy)) {
        furi_thread_yield();
    };
}

static void __isr __not_in_flash_func(display_jd9853_te_callback)(void* ctx) {
    DisplayJd9853QSPI* display = (DisplayJd9853QSPI*)ctx;
    furi_hal_gpio_write(&gpio_display_cs, false);
    display_jd9853_dma_put_buffer(display, (uint8_t*)&display->buffer_header, sizeof(display->buffer_header));
}

static int64_t __isr __not_in_flash_func(display_jd9853_end_tx_hstx_callback)(alarm_id_t id, __unused void* user_data) {
    UNUSED(id);
    furi_hal_gpio_write(&gpio_display_cs, true);
    furi_hal_gpio_disable_int_callback(&gpio_display_te);
    furi_semaphore_release(display_instance->busy);
    return 0;
}

static void __isr __not_in_flash_func(display_jd9853_dma_irq_handler)(void) {
    add_alarm_in_us(DISPLAY_JD9853_HSTX_END_TX_DELAY_US, display_jd9853_end_tx_hstx_callback, NULL, true);
    // Clear the interrupt request.
    dma_hw->ints3 = 1u << display_instance->dma_tx_channel;
}

static void display_jd9853_hstx_clock_init(void) {
    clock_configure(clk_hstx, 0, CLOCKS_CLK_HSTX_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB, USB_CLK_HZ, USB_CLK_HZ);
}

void display_jd9853_qspi_on_sleep_enter(void) {
}

void display_jd9853_qspi_on_sleep_exit(void) {
    display_jd9853_hstx_clock_init();
}

bool display_jd9853_qspi_check_connection(DisplayJd9853QSPI* display, uint32_t timeout_ms) {
    furi_check(display);
    int64_t start_time = furi_get_tick();

    while(!furi_hal_gpio_read(&gpio_display_te)) {
        if(furi_get_tick() - start_time > timeout_ms) {
            return false;
        }
        furi_thread_yield();
    };
    return true;
}

DisplayJd9853QSPI* display_jd9853_qspi_init(void) {
    furi_check(display_instance == NULL); // Only one instance allowed
    DisplayJd9853QSPI* display = malloc(sizeof(DisplayJd9853QSPI));
    display_instance = display;
    display->busy = furi_semaphore_alloc(1, 1);
#ifdef DISPLAY_JD9853_DYNAMIC_REFRESH
    display->refresh_step = DisplayJd9853RefreshMax;
    display->refresh_pending = DisplayJd9853RefreshNone;
    display->frames_in_window = 0;
    display->refresh_backpressure = false;
    display->refresh_hold_until = 0;
    display->last_frame_us = 0;
    display->fast_frames = 0;
    display->refresh_timer = NULL;
#endif
#ifdef DISPLAY_JD9853_SHOW_FPS
    display->frames_written = 0;

    display->fps_timer = furi_timer_alloc(display_jd9853_fps_timer_callback, FuriTimerTypePeriodic, NULL);
    furi_timer_start(display->fps_timer, 1000);
#endif
    display->buffer_header.cmd[0] = JD9853_QSPI_CMD_4_LINE_MODE;
    display->buffer_header.cmd[1] = 0;
    display->buffer_header.cmd[2] = JD9853_QSPI_CMD_4_LINE_RAMWR;
    display->buffer_header.cmd[3] = 0;

    //dma init
    display->dma_tx_channel = dma_claim_unused_channel(true);
    furi_check(dma_channel_is_claimed(display->dma_tx_channel));
    dma_channel_config c = dma_channel_get_default_config(display->dma_tx_channel);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_dreq(&c, DREQ_HSTX);
    channel_config_set_bswap(&c, true); // Byte swap for little-endian data
    dma_channel_set_write_addr(display->dma_tx_channel, &hstx_fifo_hw->fifo, false);
    dma_channel_set_config(display->dma_tx_channel, &c, false);

    // Set up the DMA_IRQ_3 not used SDK
    hw_set_bits(&dma_hw->inte3, 1u << display->dma_tx_channel);
    irq_set_exclusive_handler(DMA_IRQ_3, display_jd9853_dma_irq_handler);
    irq_set_enabled(DMA_IRQ_3, true);

    // Configure HSTX clock
    display_jd9853_hstx_clock_init();

    //Gpio init
    furi_hal_gpio_init_simple(&gpio_display_reset, GpioModeOutputPushPull);
    furi_hal_gpio_init_simple(&gpio_display_te, GpioModeInput);
    furi_hal_gpio_add_int_callback(&gpio_display_te, GpioConditionRise, display_jd9853_te_callback, display);
    furi_hal_gpio_disable_int_callback(&gpio_display_te);
    furi_hal_gpio_init_simple(&gpio_display_cs, GpioModeOutputPushPull);
    furi_hal_gpio_write(&gpio_display_cs, true);

    //Reset display
    furi_hal_gpio_write(&gpio_display_reset, false);
    furi_delay_ms(30);
    furi_hal_gpio_write(&gpio_display_reset, true);
    furi_delay_ms(DISPLAY_JD9853_RESET_DELAY_MS);

    //todo set gpio functions add implement furi hal gpio
    furi_hal_gpio_set_function(&gpio_display_scl, GpioAltFn0Hstx);
    furi_hal_gpio_set_function(&gpio_display_sda, GpioAltFn0Hstx);
    furi_hal_gpio_set_function(&gpio_display_cs, GpioAltFn0Hstx);
    furi_hal_gpio_set_function(&gpio_display_d0, GpioAltFn0Hstx);
    furi_hal_gpio_set_function(&gpio_display_d1, GpioAltFn0Hstx);
    furi_hal_gpio_set_function(&gpio_display_d2, GpioAltFn0Hstx);

    //Initialization sequence
    display_jd9853_load_config(display, jd9853_init_seq_2025_04_01_normal_white_mod);

    display->display_is_connected = display_jd9853_qspi_check_connection(display, DISPLAY_JD9853_CONNECTION_CHECK_TIMEOUT_MS);
    if(!display->display_is_connected) {
        FURI_LOG_E(TAG, "Display not connected");
    }

    // Clear display before enable to avoid garbage on screen
    display_jd9853_qspi_clear(display);

    display_jd9853_enable(display, true);

#ifdef DISPLAY_JD9853_DYNAMIC_REFRESH
    display->refresh_timer = furi_timer_alloc(display_jd9853_refresh_timer_callback, FuriTimerTypeOnce, display);
    furi_timer_start(display->refresh_timer, furi_ms_to_ticks(DISPLAY_JD9853_REFRESH_CHECK_INTERVAL_MS));
#endif

    return display;
}

void display_jd9853_qspi_deinit(DisplayJd9853QSPI* display) {
    furi_check(display);

#ifdef DISPLAY_JD9853_DYNAMIC_REFRESH
    if(display->refresh_timer) {
        furi_timer_stop(display->refresh_timer);
        furi_timer_free(display->refresh_timer);
        display->refresh_timer = NULL;
    }
    display->refresh_pending = DisplayJd9853RefreshNone;
#endif
#ifdef DISPLAY_JD9853_SHOW_FPS
    furi_timer_stop(display->fps_timer);
    furi_timer_free(display->fps_timer);
#endif

    bool idle = furi_semaphore_acquire(display->busy, furi_ms_to_ticks(DISPLAY_JD9853_DEINIT_TIMEOUT_MS)) == FuriStatusOk;
    furi_hal_gpio_remove_int_callback(&gpio_display_te);
    display_jd9853_load_config(display, jd9853_deinit_seq);
    if(idle) {
        furi_semaphore_release(display->busy);
    }
    furi_hal_gpio_init_ex(&gpio_display_reset, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_init_ex(&gpio_display_cs, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_init_ex(&gpio_display_sda, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_init_ex(&gpio_display_scl, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_init_ex(&gpio_display_d0, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_init_ex(&gpio_display_d1, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_init_ex(&gpio_display_d2, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);

    clock_stop(clk_hstx);
    furi_semaphore_free(display->busy);

    //deinit dma
    irq_set_enabled(DMA_IRQ_3, false);
    irq_remove_handler(DMA_IRQ_3, display_jd9853_dma_irq_handler);
    hw_clear_bits(&dma_hw->inte3, 1u << display->dma_tx_channel);
    dma_channel_unclaim(display->dma_tx_channel);

    free(display);
    display_instance = NULL;
}

void display_jd9853_qspi_eco_mode(DisplayJd9853QSPI* display, bool enable) {
    furi_check(display);
    furi_check(furi_semaphore_acquire(display->busy, FuriWaitForever) == FuriStatusOk);
    display_jd9853_hstx_init_1_line(display);
    if(enable) {
        display_jd9853_write_reg(display, idmon);
    } else {
        display_jd9853_write_reg(display, idmoff);
    }
    display_jd9853_hstx_init_4_line(display);
    furi_semaphore_release(display->busy);
}

bool display_jd9853_qspi_is_init(void) {
    return display_instance != NULL;
}
