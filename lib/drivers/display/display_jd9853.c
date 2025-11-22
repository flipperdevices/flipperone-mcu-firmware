#include "display_jd9853.h"
#include "display_jd9853_reg.h"

#include <furi_hal_gpio.h>
#include <furi_hal_resources.h>
#include <furi_hal_pwm.h>

#include "hardware/structs/clocks.h"
#include "hardware/structs/hstx_ctrl.h"
#include "hardware/structs/hstx_fifo.h"
#include <hardware/structs/io_bank0.h>
#include <pico/types.h>
#include <hardware/dma.h>

#include <hardware/clocks.h>
#include <pico/stdlib.h>

#define FIRST_HSTX_PIN 12
#define DISPLAY_JD9853_HSTX_END_TX_DELAY_US 5   //5us
#define DISPLAY_JD9853_BACKLIGHT_BIT 8          //8-bit PWM for backlight
#define DISPLAY_JD9853_BACKLIGHT_FREQ_HZ 40000  //25kHz PWM for backlight

typedef struct {
    uint32_t cmd[4];
    uint8_t data[JD9853_WIDTH * JD9853_HEIGHT];
} DisplayJd9853BufferHeader;


struct DisplayJd9853 {
    FuriSemaphore* busy;
    uint32_t dma_tx_channel;
    FuriHalPwm* backlight_pwm;
    uint8_t backlight;
    DisplayJd9853BufferHeader buffer_header;
};

static DisplayJd9853* display_instance = NULL;

static FURI_ALWAYS_INLINE void display_jd9853_hstx_wait_complete(DisplayJd9853* display) {
    while (!(hstx_fifo_hw->stat & HSTX_FIFO_STAT_EMPTY_BITS)){
        tight_loop_contents();
    }
}

static FURI_ALWAYS_INLINE void display_jd9853_hstx_init_1_line(DisplayJd9853* display) {
    
    display_jd9853_hstx_wait_complete(display);
    
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

    gpio_set_function(gpio_display_cs.pin, GPIO_FUNC_HSTX);
}

static FURI_ALWAYS_INLINE void display_jd9853_hstx_init_4_line(DisplayJd9853* display) {
   
    display_jd9853_hstx_wait_complete(display);
    
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
    
    gpio_set_function(gpio_display_cs.pin, GPIO_FUNC_SIO);

}

static FURI_ALWAYS_INLINE void display_jd9853_hstx_put_word(uint32_t data) {
	while (hstx_fifo_hw->stat & HSTX_FIFO_STAT_FULL_BITS){
        tight_loop_contents();
    }
	hstx_fifo_hw->fifo = data;
}

static FURI_ALWAYS_INLINE void display_jd9853_dma_put_buffer(DisplayJd9853* display, const uint8_t* data, size_t size) {
    display_jd9853_hstx_wait_complete(display);
    
    dma_channel_set_read_addr(display->dma_tx_channel, data, false);
    dma_channel_set_transfer_count(display->dma_tx_channel, size/4, false);

    // Start DMA transfer
    dma_channel_start(display->dma_tx_channel);
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

static FURI_ALWAYS_INLINE void display_jd9853_write_reg(DisplayJd9853* display, DisplayJd9853Reg reg) {
    display_jd9853_cs_up();
    display_jd9853_write_reg_1line(display, reg);
}

static FURI_ALWAYS_INLINE void display_jd9853_write_data(DisplayJd9853* display, uint8_t* data, size_t size) {
    for(size_t i = 0; i < size; i++) {
        display_jd9853_hstx_put_word(data[i]);
    }
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

FURI_ALWAYS_INLINE void display_jd9853_write_buffer(DisplayJd9853* display, const uint8_t* buffer, size_t size) {
    furi_assert(display);
    furi_check(size == JD9853_WIDTH * JD9853_HEIGHT); //size must be equal to full buffer size

    while (furi_semaphore_get_space(display->busy)) {
        tight_loop_contents();
    };

    memcpy(display->buffer_header.data, buffer, size);

    furi_check(furi_semaphore_acquire(display->busy, FuriWaitForever) == FuriStatusOk);
}

void display_jd9853_fill(DisplayJd9853* display, uint8_t color) {
    furi_assert(display);
    const size_t width = JD9853_WIDTH; // 1 byte per pixel
    const size_t height = JD9853_HEIGHT;

    uint8_t* data = (uint8_t*)malloc(width * height);
    for(size_t i = 0; i < width * height; i += 1) {
        data[i] = color;
    }

    display_jd9853_write_buffer(display, data, width * height);
    free(data);
}

static void __isr __not_in_flash_func(display_jd9853_te_callback)(void* ctx) {
    DisplayJd9853* display = (DisplayJd9853*)ctx;
    if(!furi_semaphore_get_space(display->busy)) {
        return;
    }
    furi_hal_gpio_write(&gpio_display_cs, false); 
    display_jd9853_dma_put_buffer(display, (uint8_t*)&display->buffer_header, sizeof(display->buffer_header));
}

static int64_t __isr __not_in_flash_func(display_jd9853_end_tx_hstx_callback)(alarm_id_t id, __unused void *user_data) {
    furi_hal_gpio_write(&gpio_display_cs, true);
    furi_semaphore_release(display_instance->busy);
    return 0;
}

static void __isr __not_in_flash_func(display_jd9853_dma_irq_handler)(void) {
    add_alarm_in_us(DISPLAY_JD9853_HSTX_END_TX_DELAY_US, display_jd9853_end_tx_hstx_callback, NULL, true);
    // Clear the interrupt request.
    dma_hw->ints3 = 1u << display_instance->dma_tx_channel;
}

void display_jd9853_hstx_clock_init(void) {
    clock_configure(clk_hstx,
                        0,
                        CLOCKS_CLK_HSTX_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
                        USB_CLK_HZ,
                        USB_CLK_HZ);
}

void display_jd9853_backlight_set_brightness(DisplayJd9853* display, uint8_t brightness) {
    furi_check(display);
    display->backlight = brightness;
    if(!display->backlight){
        if(display->backlight_pwm){
            furi_hal_pwm_set_duty_cycle(display->backlight_pwm, 0);
            furi_hal_pwm_deinit(display->backlight_pwm);
            display->backlight_pwm = NULL;
        }
    } else{
        uint32_t max_value = (1 << DISPLAY_JD9853_BACKLIGHT_BIT) - 1;
        uint32_t duty_cycle = (brightness * max_value) / 100;
        if(!display->backlight_pwm){
            //To enable the device, the CTRL signal must be high for 500 µs.
            // The PWM signal can then be applied with a pulse width (tp) 
            // greater or smaller than tON. To force the device into shutdown mode,
            // the CTRL signal must be low for at least 32 ms. 
            // Requiring the CTRL pin to be low for 32 mS before the device enters 
            // shutdown allows for PWM dimming frequencies as low as 100 Hz.
            // The device is enabled again when a CTRL signal is high for a period of 500 µs minimum.
            display->backlight_pwm = furi_hal_pwm_init(&gpio_display_ctrl, DISPLAY_JD9853_BACKLIGHT_BIT, DISPLAY_JD9853_BACKLIGHT_FREQ_HZ, false);
            furi_hal_pwm_set_duty_cycle(display->backlight_pwm, 140);
            furi_delay_us(2400);
        }
        furi_hal_pwm_set_duty_cycle(display->backlight_pwm, duty_cycle);
    }
}

uint8_t display_jd9853_backlight_get_brightness(DisplayJd9853* display) {
    furi_check(display);
    return display->backlight;
}

DisplayJd9853* display_jd9853_init(void) {
    furi_check(display_instance == NULL); // Only one instance allowed
    DisplayJd9853* display = malloc(sizeof(DisplayJd9853));
    display_instance = display;
    display->busy = furi_semaphore_alloc(1, 1);
    display->backlight = 0;

    display->buffer_header.cmd[0] = JD9853_QSPI_CMD_4_LINE_MODE;
    display->buffer_header.cmd[1] = 0;
    display->buffer_header.cmd[2] = JD9853_QSPI_CMD_4_LINE_RAMWR;
    display->buffer_header.cmd[3] = 0;

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
    display_jd9853_hstx_clock_init();
    
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
    display_jd9853_fill(display, 0); // Fill white

    display_jd9853_backlight_set_brightness(display, 2); // Set backlight to 50%

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
    furi_semaphore_free(display->busy);

    //deinit dma
    irq_set_enabled(DMA_IRQ_3, false);
    irq_remove_handler(DMA_IRQ_3, display_jd9853_dma_irq_handler);
    hw_clear_bits(&dma_hw->inte3, 1u << display->dma_tx_channel);
    dma_channel_unclaim(display->dma_tx_channel);

    free(display);
    display_instance = NULL;
}

void display_jd9853_eco_mode(DisplayJd9853* display, bool enable) {
    furi_check(display);
    display_jd9853_hstx_init_1_line(display);
    if(enable) {
        display_jd9853_write_reg(display, idmon);
    } else {
        display_jd9853_write_reg(display, idmoff);
    }
    display_jd9853_hstx_init_4_line(display);
}
