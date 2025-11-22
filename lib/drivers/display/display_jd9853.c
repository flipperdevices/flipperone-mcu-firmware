#include "display_jd9853.h"
#include "core/check.h"
#include "core/kernel.h"
#include "core/log.h"
#include "display_jd9853_reg.h"

#include <furi_hal_gpio.h>
#include <furi_hal_resources.h>

#include "hardware/structs/clocks.h"
#include "hardware/structs/hstx_ctrl.h"
#include "hardware/structs/hstx_fifo.h"
#include <hardware/structs/io_bank0.h>
#include <pico/types.h>
#include <hardware/dma.h>

#include <hardware/clocks.h>
#include <hardware/timer.h>
#include "pico/stdlib.h"
#include <stdio.h>

#define FIRST_HSTX_PIN 12
#define DISPLAY_JD9853_TE_TIMEOUT_DELTA 4 //4ms 
#define DISPLAY_JD9853_HSTX_END_TX_DELAY_US 5 //5us

typedef enum {
    DisplayJd9853Line1,
    DisplayJd9853Line2,
    DisplayJd9853Line4,
} DisplayJd9853Line;

// typedef struct {
//     uint32_t cmd;
//     uint8_t data[];
// } DisplayJd9853BufferHeader;


struct DisplayJd9853 {
    FuriSemaphore* te_semaphore;
    DisplayJd9853Line line_mode;
    volatile uint32_t te_timestamp;
    uint32_t dma_tx_channel;
};

static DisplayJd9853* display_instance = NULL;

static FURI_ALWAYS_INLINE void display_jd9853_hstx_wait_complete(DisplayJd9853* display) {
    while (!(hstx_fifo_hw->stat & HSTX_FIFO_STAT_EMPTY_BITS))
        ;
}

static FURI_ALWAYS_INLINE void display_jd9853_hstx_init_1_line(DisplayJd9853* display) {
    
   // display_jd9853_hstx_wait_complete(display);
    
    hstx_ctrl_hw->bit[gpio_display_scl.pin - FIRST_HSTX_PIN] =
        HSTX_CTRL_BIT0_CLK_BITS;

    hstx_ctrl_hw->bit[gpio_display_sda.pin - FIRST_HSTX_PIN] =
        (7u << HSTX_CTRL_BIT0_SEL_P_LSB) |
        (7u << HSTX_CTRL_BIT0_SEL_N_LSB);
    hstx_ctrl_hw->bit[gpio_display_d0.pin - FIRST_HSTX_PIN] =
        (7u << HSTX_CTRL_BIT0_SEL_P_LSB) |
        (7u << HSTX_CTRL_BIT0_SEL_N_LSB);

    hstx_ctrl_hw->bit[gpio_display_d1.pin - FIRST_HSTX_PIN] =
        (7u << HSTX_CTRL_BIT0_SEL_P_LSB) |
        (7u << HSTX_CTRL_BIT0_SEL_N_LSB);
    hstx_ctrl_hw->bit[gpio_display_d2.pin - FIRST_HSTX_PIN] =
        (7u << HSTX_CTRL_BIT0_SEL_P_LSB) |
        (7u << HSTX_CTRL_BIT0_SEL_N_LSB);

    hstx_ctrl_hw->bit[gpio_display_cs.pin - FIRST_HSTX_PIN] =
        (27u << HSTX_CTRL_BIT0_SEL_P_LSB) |
        (27u << HSTX_CTRL_BIT0_SEL_N_LSB);

    //We have packed 8-bit fields, so shift left 1 bit/cycle, 8 times.
    hstx_ctrl_hw->csr =
        HSTX_CTRL_CSR_EN_BITS |
        (31u << HSTX_CTRL_CSR_SHIFT_LSB) |  
        (8u << HSTX_CTRL_CSR_N_SHIFTS_LSB) | 
        (1u << HSTX_CTRL_CSR_CLKDIV_LSB);

    display->line_mode = DisplayJd9853Line1;
    gpio_set_function(gpio_display_cs.pin, GPIO_FUNC_HSTX);
}

static FURI_ALWAYS_INLINE void display_jd9853_hstx_init_2_line(DisplayJd9853* display) {
    
    display_jd9853_hstx_wait_complete(display);

    hstx_ctrl_hw->bit[gpio_display_scl.pin - FIRST_HSTX_PIN] =
        HSTX_CTRL_BIT0_CLK_BITS;

    hstx_ctrl_hw->bit[gpio_display_sda.pin - FIRST_HSTX_PIN] =
        (6u << HSTX_CTRL_BIT0_SEL_P_LSB) |
        (6u << HSTX_CTRL_BIT0_SEL_N_LSB);
    hstx_ctrl_hw->bit[gpio_display_d0.pin - FIRST_HSTX_PIN] =
        (7u << HSTX_CTRL_BIT0_SEL_P_LSB) |
        (7u << HSTX_CTRL_BIT0_SEL_N_LSB);

    hstx_ctrl_hw->bit[gpio_display_d1.pin - FIRST_HSTX_PIN] =
        (6u << HSTX_CTRL_BIT0_SEL_P_LSB) |
        (6u << HSTX_CTRL_BIT0_SEL_N_LSB);
    hstx_ctrl_hw->bit[gpio_display_d2.pin - FIRST_HSTX_PIN] =
        (7u << HSTX_CTRL_BIT0_SEL_P_LSB) |
        (7u << HSTX_CTRL_BIT0_SEL_N_LSB);

    hstx_ctrl_hw->bit[gpio_display_cs.pin - FIRST_HSTX_PIN] =
        (27u << HSTX_CTRL_BIT0_SEL_P_LSB) |
        (27u << HSTX_CTRL_BIT0_SEL_N_LSB);

    //We have packed 8-bit fields, so shift left 2 bit/cycle, 4 times.
    hstx_ctrl_hw->csr =
        HSTX_CTRL_CSR_EN_BITS |
        (30u << HSTX_CTRL_CSR_SHIFT_LSB) | 
        (4u << HSTX_CTRL_CSR_N_SHIFTS_LSB) |
        (1u << HSTX_CTRL_CSR_CLKDIV_LSB);

    display->line_mode = DisplayJd9853Line2;
}

static FURI_ALWAYS_INLINE void display_jd9853_hstx_init_4_line(DisplayJd9853* display) {
   
    display_jd9853_hstx_wait_complete(display);
    //hstx_ctrl_hw->csr = ~HSTX_CTRL_CSR_EN_BITS ;
    
    hstx_ctrl_hw->bit[gpio_display_scl.pin - FIRST_HSTX_PIN] =
        HSTX_CTRL_BIT0_CLK_BITS;

    hstx_ctrl_hw->bit[gpio_display_sda.pin - FIRST_HSTX_PIN] =
        (28u << HSTX_CTRL_BIT0_SEL_P_LSB) |
        (28u << HSTX_CTRL_BIT0_SEL_N_LSB);
    hstx_ctrl_hw->bit[gpio_display_d0.pin - FIRST_HSTX_PIN] =
        (29u << HSTX_CTRL_BIT0_SEL_P_LSB) |
        (29u << HSTX_CTRL_BIT0_SEL_N_LSB);

    hstx_ctrl_hw->bit[gpio_display_d1.pin - FIRST_HSTX_PIN] =
        (30u << HSTX_CTRL_BIT0_SEL_P_LSB) |
        (30u << HSTX_CTRL_BIT0_SEL_N_LSB);
    hstx_ctrl_hw->bit[gpio_display_d2.pin - FIRST_HSTX_PIN] =
        (31u << HSTX_CTRL_BIT0_SEL_P_LSB) |
        (31u << HSTX_CTRL_BIT0_SEL_N_LSB);

    hstx_ctrl_hw->bit[gpio_display_cs.pin - FIRST_HSTX_PIN] = 0x00;
    //We have packed 32-bit fields, so shift left 4 bit/cycle, 8 times.
    hstx_ctrl_hw->csr =
        HSTX_CTRL_CSR_EN_BITS |
        (28u << HSTX_CTRL_CSR_SHIFT_LSB) |  
        (8u << HSTX_CTRL_CSR_N_SHIFTS_LSB) | 
        (1u << HSTX_CTRL_CSR_CLKDIV_LSB);
    
    display->line_mode = DisplayJd9853Line4;
    gpio_set_function(gpio_display_cs.pin, GPIO_FUNC_SIO);

}

static FURI_ALWAYS_INLINE void display_jd9853_hstx_put_word(uint32_t data) {
	while (hstx_fifo_hw->stat & HSTX_FIFO_STAT_FULL_BITS)
		;
	hstx_fifo_hw->fifo = data;
}

static FURI_ALWAYS_INLINE void display_jd9853_dma_put_buffer(DisplayJd9853* display, const uint8_t* data, size_t size) {
    display_jd9853_hstx_wait_complete(display);

    dma_channel_set_read_addr(display->dma_tx_channel, data, false);
    dma_channel_set_transfer_count(display->dma_tx_channel, size/4, false);


    // Start DMA transfer
    dma_channel_start(display->dma_tx_channel);

    // Wait for DMA transfer to complete
    dma_channel_wait_for_finish_blocking(display->dma_tx_channel);
    //furi_delay_ms(2);
}

static FURI_ALWAYS_INLINE void display_jd9853_cs_up(void) {
	display_jd9853_hstx_put_word(0x0ff00000u);
}

static FURI_ALWAYS_INLINE void display_jd9853_write_reg_1line(DisplayJd9853* display, DisplayJd9853Reg reg){
    display_jd9853_hstx_put_word(JD9853_QSPI_CMD_1_LINE_MODE); // Command Write Quad SPI
    display_jd9853_hstx_put_word((uint8_t)0x00);
    display_jd9853_hstx_put_word((uint8_t)reg);
    display_jd9853_hstx_put_word((uint8_t)0x00);
}


uint32_t convert_to_dual_line_compact(uint8_t data, uint8_t line ) {
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

static FURI_ALWAYS_INLINE void display_jd9853_write_reg_2line(DisplayJd9853* display, DisplayJd9853Reg reg){

    uint32_t reg_16 = convert_to_dual_line_compact(JD9853_QSPI_CMD_2_LINE_MODE, 2); // Command Write Quad SPI
    
    display_jd9853_hstx_put_word((uint8_t)(reg_16>>8));
    display_jd9853_hstx_put_word((uint8_t)(reg_16 & 0xFF));
    display_jd9853_hstx_put_word((uint8_t)0x00);
    display_jd9853_hstx_put_word((uint8_t)0x00);

    reg_16 = convert_to_dual_line_compact((uint8_t)reg, 2);
    display_jd9853_hstx_put_word((uint8_t)(reg_16>>8));
    display_jd9853_hstx_put_word((uint8_t)(reg_16 & 0xFF));

    display_jd9853_hstx_put_word((uint8_t)0x00);
    display_jd9853_hstx_put_word((uint8_t)0x00);
}

static FURI_ALWAYS_INLINE void display_jd9853_write_reg_4line(DisplayJd9853* display, DisplayJd9853Reg reg){

    uint32_t reg_16 = convert_to_dual_line_compact(JD9853_QSPI_CMD_4_LINE_MODE, 4); // Command Write Quad SPI
    display_jd9853_hstx_put_word(reg_16);

    display_jd9853_hstx_put_word(0x00000000u);

    reg_16 = convert_to_dual_line_compact((uint8_t)reg, 4);
    display_jd9853_hstx_put_word(reg_16);

    display_jd9853_hstx_put_word(0x00000000u);
}

static FURI_ALWAYS_INLINE void display_jd9853_write_reg(DisplayJd9853* display, DisplayJd9853Reg reg) {
    display_jd9853_cs_up();
    switch (display->line_mode) {
    case DisplayJd9853Line1:
        display_jd9853_write_reg_1line(display, reg);
        break;
    case DisplayJd9853Line2:
        display_jd9853_write_reg_2line(display, reg);
        break;
    case DisplayJd9853Line4:
        display_jd9853_write_reg_4line(display, reg);
        break;
    default:
        display_jd9853_write_reg_1line(display, reg);
        break;
        
    }
}

static FURI_ALWAYS_INLINE void display_jd9853_write_data(DisplayJd9853* display, uint8_t* data, size_t size) {
    for(size_t i = 0; i < size; i++) {
        display_jd9853_hstx_put_word(data[i]);
    }
    //display_jd9853_cs_up();
}

static FURI_ALWAYS_INLINE void display_jd9853_load_config(DisplayJd9853* display, const uint8_t* config) {
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

static FURI_ALWAYS_INLINE void display_jd9853_set_window(DisplayJd9853* display, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t caset_data[4] = {(uint8_t)(x0 >> 8), (uint8_t)(x0 & 0xFF), (uint8_t)(x1 >> 8), (uint8_t)(x1 & 0xFF)};
    uint8_t paset_data[4] = {(uint8_t)(y0 >> 8), (uint8_t)(y0 & 0xFF), (uint8_t)(y1 >> 8), (uint8_t)(y1 & 0xFF)};

    display_jd9853_hstx_init_1_line(display);
    
    display_jd9853_write_reg(display, caset); // Column address set
    display_jd9853_write_data(display, caset_data, sizeof(caset_data));

    display_jd9853_write_reg(display, paset); // Page address set
    display_jd9853_write_data(display, paset_data, sizeof(paset_data));
    
    display_jd9853_hstx_init_4_line(display);
}

FURI_ALWAYS_INLINE void display_jd9853_write_buffer_x_y(DisplayJd9853* display, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t* buffer, size_t size) {
    furi_assert(display);
    //furi_check(furi_semaphore_acquire(display->te_semaphore, FuriWaitForever) == FuriStatusOk);
    //furi_check(furi_semaphore_acquire(display->te_semaphore, FuriWaitForever) == FuriStatusOk);
    while( (furi_get_tick() - display->te_timestamp) > DISPLAY_JD9853_TE_TIMEOUT_DELTA ) {
        //wait for next TE signal
    }
    //display_jd9853_set_window(display, JD9853_OFF_X0 + x, JD9853_OFF_Y0 + y, JD9853_OFF_X0 + x + (w / 3)-1, JD9853_OFF_Y0 + y + h - 1);
    furi_hal_gpio_write(&gpio_display_cs, false); 


    display_jd9853_write_reg_4line(display, ramwr);
    //display_jd9853_write_reg(display, ramwr);
    //display_jd9853_write_data(display, (uint8_t*)buffer, size);

    display_jd9853_dma_put_buffer(display, buffer, size);

    // furi_hal_gpio_write(&gpio_display_cs, true); 
    
    // display_jd9853_hstx_wait_complete(display);
    //display_jd9853_cs_up();
}

FURI_ALWAYS_INLINE void display_jd9853_write_buffer(DisplayJd9853* display, uint16_t w, uint16_t h, const uint8_t* buffer, size_t size) {
    furi_assert(display);
    display_jd9853_write_buffer_x_y(display, 0, 0, w, h, buffer, size);
}

uint8_t d = 0;
void display_jd9853_fill(DisplayJd9853* display, uint8_t color) {
    furi_assert(display);
    const size_t width = JD9853_WIDTH; // 1 byte per pixel
    const size_t height = JD9853_HEIGHT;

    uint8_t* data = (uint8_t*)malloc(width * height);
    for(size_t i = 0; i < width * height; i += 1) {
        data[i] = ((d+i)%64) << 2;
        //data[i] = color;
    }
    d++;
    if(d >= 64) {
        d = 0;
    }
    

    display_jd9853_write_buffer(display, width, height, data, width * height);
    free(data);
}

static void __isr __not_in_flash_func(display_jd9853_te_callback)(void* ctx) {
    DisplayJd9853* display = (DisplayJd9853*)ctx;
    display->te_timestamp = furi_get_tick();
    furi_semaphore_release(display->te_semaphore);
}

static int64_t __isr __not_in_flash_func(display_jd9853_end_tx_hstx_callback)(alarm_id_t id, __unused void *user_data) {
    furi_hal_gpio_write(&gpio_display_cs, true);
    return 0;
}

static void __isr __not_in_flash_func(display_jd9853_dma_irq_handler)(void) {
    add_alarm_in_us(DISPLAY_JD9853_HSTX_END_TX_DELAY_US, display_jd9853_end_tx_hstx_callback, NULL, true);
    // Clear the interrupt request.
    dma_hw->ints3 = 1u << display_instance->dma_tx_channel;
}

DisplayJd9853* display_jd9853_init(void) {
    furi_check(display_instance == NULL); // Only one instance allowed
    DisplayJd9853* display = malloc(sizeof(DisplayJd9853));
    display_instance = display;
    display->te_semaphore = furi_semaphore_alloc(1, 1);

    //dma init
    display->dma_tx_channel=dma_claim_unused_channel(true);
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
    clock_configure(clk_hstx,
                        0,
                        CLOCKS_CLK_HSTX_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
                        USB_CLK_HZ,
                        USB_CLK_HZ/2);
    
    
    //Gpio init
    //furi_hal_gpio_init_simple(display->pin_reset, GpioModeOutputOpenDrain);
    furi_hal_gpio_init_simple(&gpio_display_reset, GpioModeOutputPushPull);
    furi_hal_gpio_init_simple(&gpio_display_te, GpioModeInput);
    furi_hal_gpio_add_int_callback(&gpio_display_te, GpioConditionRise, display_jd9853_te_callback, display);
    furi_hal_gpio_init_simple(&gpio_display_cs, GpioModeOutputPushPull); 
    furi_hal_gpio_write(&gpio_display_cs, true);   

    //Reset display
    //ToDo return to open drain after testing
    // furi_hal_gpio_write_open_drain(display->pin_reset, false);
    // furi_delay_ms(30);
    // furi_hal_gpio_write_open_drain(display->pin_reset, true);
    // furi_delay_ms(30);
    furi_hal_gpio_write(&gpio_display_reset, false);
    furi_delay_ms(30);
    furi_hal_gpio_write(&gpio_display_reset, true);
    furi_delay_ms(30);
    
    
    
    //todo set gpio functions add implement furi hal gpio
    gpio_set_function(gpio_display_scl.pin, GPIO_FUNC_HSTX);
    gpio_set_function(gpio_display_sda.pin, GPIO_FUNC_HSTX);
    gpio_set_function(gpio_display_cs.pin, GPIO_FUNC_HSTX);
    gpio_set_function(gpio_display_d0.pin, GPIO_FUNC_HSTX);
    gpio_set_function(gpio_display_d1.pin, GPIO_FUNC_HSTX);
    gpio_set_function(gpio_display_d2.pin, GPIO_FUNC_HSTX);


    //Initialization sequence
    display_jd9853_load_config(display, jd9853_init_seq_2025_04_01_normal_black);
    //display_jd9853_load_config(display, jd9853_init_seq_2025_04_01_normal_black);
    //display_jd9853_set_window(display, JD9853_OFF_X0, JD9853_OFF_Y0, JD9853_OFF_X0 + (JD9853_WIDTH / 3)-1, JD9853_OFF_Y0 + JD9853_HEIGHT - 1);
    display_jd9853_fill(display, 255); // Fill white

    return display;
}

void display_jd9853_deinit(DisplayJd9853* display) {
    furi_check(display);
    
    furi_hal_gpio_remove_int_callback(&gpio_display_te);
    display_jd9853_load_config(display, st7789_deinit_seq);
    furi_hal_gpio_init_ex(&gpio_display_reset, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_init_ex(&gpio_display_cs, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_init_ex(&gpio_display_sda, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_init_ex(&gpio_display_scl, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_init_ex(&gpio_display_d0, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_init_ex(&gpio_display_d1, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_init_ex(&gpio_display_d2, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    
    clock_stop(clk_hstx);
    furi_semaphore_free(display->te_semaphore);
    //deinit dma
    irq_set_enabled(DMA_IRQ_3, false);
    irq_remove_handler(DMA_IRQ_3, display_jd9853_dma_irq_handler);
    hw_clear_bits(&dma_hw->inte3, 1u << display->dma_tx_channel);
    dma_channel_unclaim(display->dma_tx_channel);

    free(display);
    display_instance = NULL;
}

