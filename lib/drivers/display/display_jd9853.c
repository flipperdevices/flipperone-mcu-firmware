#include "display_jd9853.h"
#include "display_jd9853_reg.h"

#include <furi_hal_gpio.h>
#include <furi_hal_resources.h>

#include "hardware/structs/clocks.h"
#include "hardware/structs/hstx_ctrl.h"
#include "hardware/structs/hstx_fifo.h"
#include <pico/types.h>

#include <hardware/clocks.h>

#define FIRST_HSTX_PIN 12

typedef enum {
    DisplayJd9853Line1,
    DisplayJd9853Line2,
    DisplayJd9853Line4,
} DisplayJd9853Line;

struct DisplayJd9853 {
    DisplayJd9853Line line_mode;
};

static FURI_ALWAYS_INLINE void display_jd9853_hstx_wait_complete(DisplayJd9853* display) {
    while (!(hstx_fifo_hw->stat & HSTX_FIFO_STAT_EMPTY_BITS))
        ;
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

    display->line_mode = DisplayJd9853Line1;
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
    
    hstx_ctrl_hw->bit[gpio_display_scl.pin - FIRST_HSTX_PIN] =
        HSTX_CTRL_BIT0_CLK_BITS;

    hstx_ctrl_hw->bit[gpio_display_sda.pin - FIRST_HSTX_PIN] =
        (4u << HSTX_CTRL_BIT0_SEL_P_LSB) |
        (4u << HSTX_CTRL_BIT0_SEL_N_LSB);
    hstx_ctrl_hw->bit[gpio_display_d0.pin - FIRST_HSTX_PIN] =
        (5u << HSTX_CTRL_BIT0_SEL_P_LSB) |
        (5u << HSTX_CTRL_BIT0_SEL_N_LSB);

    hstx_ctrl_hw->bit[gpio_display_d1.pin - FIRST_HSTX_PIN] =
        (6u << HSTX_CTRL_BIT0_SEL_P_LSB) |
        (6u << HSTX_CTRL_BIT0_SEL_N_LSB);
    hstx_ctrl_hw->bit[gpio_display_d2.pin - FIRST_HSTX_PIN] =
        (7u << HSTX_CTRL_BIT0_SEL_P_LSB) |
        (7u << HSTX_CTRL_BIT0_SEL_N_LSB);

    hstx_ctrl_hw->bit[gpio_display_cs.pin - FIRST_HSTX_PIN] =
        (27u << HSTX_CTRL_BIT0_SEL_P_LSB) |
        (27u << HSTX_CTRL_BIT0_SEL_N_LSB);
    //We have packed 8-bit fields, so shift left 4 bit/cycle, 2 times.
    hstx_ctrl_hw->csr =
        HSTX_CTRL_CSR_EN_BITS |
        (28u << HSTX_CTRL_CSR_SHIFT_LSB) |
        (2u << HSTX_CTRL_CSR_N_SHIFTS_LSB) |
        (1u << HSTX_CTRL_CSR_CLKDIV_LSB);
    
    display->line_mode = DisplayJd9853Line4;
}

static FURI_ALWAYS_INLINE void display_jd9853_hstx_put_word(uint32_t data) {
	while (hstx_fifo_hw->stat & HSTX_FIFO_STAT_FULL_BITS)
		;
	hstx_fifo_hw->fifo = data;
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
    
    display_jd9853_hstx_put_word((uint8_t)(reg_16>>24)& 0xFF);
    display_jd9853_hstx_put_word((uint8_t)(reg_16>>16)& 0xFF);
    display_jd9853_hstx_put_word((uint8_t)(reg_16>>8)& 0xFF);
    display_jd9853_hstx_put_word((uint8_t)(reg_16 & 0xFF));

    display_jd9853_hstx_put_word((uint8_t)0x00);
    display_jd9853_hstx_put_word((uint8_t)0x00);
    display_jd9853_hstx_put_word((uint8_t)0x00);
    display_jd9853_hstx_put_word((uint8_t)0x00);

    reg_16 = convert_to_dual_line_compact((uint8_t)reg, 4);
    display_jd9853_hstx_put_word((uint8_t)(reg_16>>24)& 0xFF);
    display_jd9853_hstx_put_word((uint8_t)(reg_16>>16)& 0xFF);
    display_jd9853_hstx_put_word((uint8_t)(reg_16>>8)& 0xFF);
    display_jd9853_hstx_put_word((uint8_t)(reg_16 & 0xFF));

    display_jd9853_hstx_put_word((uint8_t)0x00);
    display_jd9853_hstx_put_word((uint8_t)0x00);
    display_jd9853_hstx_put_word((uint8_t)0x00);
    display_jd9853_hstx_put_word((uint8_t)0x00);
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
    display_jd9853_cs_up();
}

static FURI_ALWAYS_INLINE void display_jd9853_load_config(DisplayJd9853* display, const uint8_t* config) {
    while(*config) {
        display_jd9853_write_reg(display, (DisplayJd9853Reg)(*(config + 2)));
        
        if(*(config)) {
            display_jd9853_write_data(display, (uint8_t*)(config + 3), *(config)-1);
        }
        furi_delay_ms(*(config + 1) * 5);
        config += *(config) + 2;
    }
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
    display_jd9853_set_window(display, JD9853_OFF_X + x, JD9853_OFF_Y + y, JD9853_OFF_X + x + (w / 3)-1, JD9853_OFF_Y + y + h - 1);
    display_jd9853_write_reg(display, ramwr);
    display_jd9853_write_data(display, (uint8_t*)buffer, size);
}

FURI_ALWAYS_INLINE void display_jd9853_write_buffer(DisplayJd9853* display, uint16_t w, uint16_t h, const uint8_t* buffer, size_t size) {
    furi_assert(display);
    display_jd9853_write_buffer_x_y(display, 0, 0, w, h, buffer, size);
}

void display_jd9853_fill(DisplayJd9853* display, uint8_t color) {
    furi_assert(display);
    const size_t width = JD9853_WIDTH; // 1 byte per pixel
    const size_t height = JD9853_HEIGHT;

    uint8_t* data = (uint8_t*)malloc(width * height);
    for(size_t i = 0; i < width * height; i += 1) {
        data[i] = color;
    }

    display_jd9853_write_buffer(display, width, height, data, width * height);
    free(data);
}

DisplayJd9853* display_jd9853_init(void) {
    DisplayJd9853* display = malloc(sizeof(DisplayJd9853));

    clock_configure(clk_hstx,
                        0,
                        CLOCKS_CLK_HSTX_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
                        USB_CLK_HZ,
                        USB_CLK_HZ);
    
    
    //Gpio init
    //furi_hal_gpio_init_simple(display->pin_reset, GpioModeOutputOpenDrain);
    furi_hal_gpio_init_simple(&gpio_display_reset, GpioModeOutputPushPull);


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

    display_jd9853_hstx_init_1_line(display);
    //Initialization sequence
    display_jd9853_load_config(display, jd9853_init_seq_2025_04_01_normal_black);
    //display_jd9853_load_config(display, jd9853_init_seq_2025_04_01_normal_black);
    display_jd9853_fill(display, 255); // Fill white

    return display;
}

void display_jd9853_deinit(DisplayJd9853* display) {
    furi_check(display);
    display_jd9853_hstx_init_1_line(display);
    display_jd9853_load_config(display, st7789_deinit_seq);
    furi_hal_gpio_init_ex(&gpio_display_reset, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_init_ex(&gpio_display_cs, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_init_ex(&gpio_display_sda, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_init_ex(&gpio_display_scl, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_init_ex(&gpio_display_d0, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_init_ex(&gpio_display_d1, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_init_ex(&gpio_display_d2, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    display_jd9853_hstx_wait_complete(display);
    clock_stop(clk_hstx);
    free(display);
}


