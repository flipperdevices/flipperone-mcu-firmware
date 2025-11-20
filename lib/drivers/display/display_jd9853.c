#include "display_jd9853.h"
#include "display_jd9853_reg.h"

#include <furi_hal_gpio.h>
#include <furi_hal_resources.h>

#include "hardware/structs/clocks.h"
#include "hardware/structs/hstx_ctrl.h"
#include "hardware/structs/hstx_fifo.h"
#include <pico/types.h>

#include <hardware/clocks.h>

struct DisplayJd9853 {
    // FuriHalSpiHandle* spi_handle;

    const GpioPin* pin_dc;
    const GpioPin* pin_reset;
    uint8_t line_mode;
};

#define FIRST_HSTX_PIN 12

static inline void hstx_put_word(uint32_t data) {
	while (hstx_fifo_hw->stat & HSTX_FIFO_STAT_FULL_BITS)
		;
	hstx_fifo_hw->fifo = data;
}

static inline void lcd_put_dc_cs_data(bool csn, uint8_t data) {
	hstx_put_word(
		(uint32_t)data |
		(csn ? 0x0ff00000u : 0x00000000u) 
	);
}

static FURI_ALWAYS_INLINE void display_jd9853_write_reg_1line(DisplayJd9853* display, DisplayJd9853Reg reg){
    lcd_put_dc_cs_data(false, (uint8_t)0x02); // Command Write Quad SPI
    lcd_put_dc_cs_data(false, (uint8_t)0x00);
    lcd_put_dc_cs_data(false, (uint8_t)reg);
    lcd_put_dc_cs_data(false, (uint8_t)0x00);
}


uint32_t convert_to_dual_line_compact(uint8_t data, uint8_t line ) {
    uint32_t result = 0;

    for(size_t i = 0; i < 8; i++) {
        result <<= line;
        if(data & 0x80) {
            result |= 0x1;
        }
        data <<= 1;
        //result <<= 1;
    }    
    return result;
}

static FURI_ALWAYS_INLINE void display_jd9853_write_reg_2line(DisplayJd9853* display, DisplayJd9853Reg reg){

    uint32_t reg_16 = convert_to_dual_line_compact((uint8_t)0xA2, 2); // Command Write Quad SPI
    
    //reg_16 = 0x5555;
    lcd_put_dc_cs_data(false, (uint8_t)(reg_16>>8));
    lcd_put_dc_cs_data(false, (uint8_t)(reg_16 & 0xFF));
    //FURI_LOG_I("F", "reg_16: 0x%04X", reg_16); //4404
    lcd_put_dc_cs_data(false, (uint8_t)0x00);
    lcd_put_dc_cs_data(false, (uint8_t)0x00);

    reg_16 = convert_to_dual_line_compact((uint8_t)reg, 2);
    //FURI_LOG_I("F", "reg_16: 0x%04X", reg_16); //0450
    lcd_put_dc_cs_data(false, (uint8_t)(reg_16>>8));
    lcd_put_dc_cs_data(false, (uint8_t)(reg_16 & 0xFF));

    lcd_put_dc_cs_data(false, (uint8_t)0x00);
    lcd_put_dc_cs_data(false, (uint8_t)0x00);
}

static FURI_ALWAYS_INLINE void display_jd9853_write_reg_4line(DisplayJd9853* display, DisplayJd9853Reg reg){

    uint32_t reg_16 = convert_to_dual_line_compact((uint8_t)0x32, 4); // Command Write Quad SPI
    
    //reg_16 = 0x5555;
    lcd_put_dc_cs_data(false, (uint8_t)(reg_16>>24)& 0xFF);
    lcd_put_dc_cs_data(false, (uint8_t)(reg_16>>16)& 0xFF);
    lcd_put_dc_cs_data(false, (uint8_t)(reg_16>>8)& 0xFF);
    lcd_put_dc_cs_data(false, (uint8_t)(reg_16 & 0xFF));
    //FURI_LOG_I("F", "reg_16: 0x%04X", reg_16); //4404
    lcd_put_dc_cs_data(false, (uint8_t)0x00);
    lcd_put_dc_cs_data(false, (uint8_t)0x00);
    lcd_put_dc_cs_data(false, (uint8_t)0x00);
    lcd_put_dc_cs_data(false, (uint8_t)0x00);

    reg_16 = convert_to_dual_line_compact((uint8_t)reg, 4);
    //FURI_LOG_I("F", "reg_16: 0x%04X", reg_16); //0450
    lcd_put_dc_cs_data(false, (uint8_t)(reg_16>>24)& 0xFF);
    lcd_put_dc_cs_data(false, (uint8_t)(reg_16>>16)& 0xFF);
    lcd_put_dc_cs_data(false, (uint8_t)(reg_16>>8)& 0xFF);
    lcd_put_dc_cs_data(false, (uint8_t)(reg_16 & 0xFF));

    lcd_put_dc_cs_data(false, (uint8_t)0x00);
    lcd_put_dc_cs_data(false, (uint8_t)0x00);
    lcd_put_dc_cs_data(false, (uint8_t)0x00);
    lcd_put_dc_cs_data(false, (uint8_t)0x00);
}

static FURI_ALWAYS_INLINE void display_jd9853_write_reg(DisplayJd9853* display, DisplayJd9853Reg reg) {
    lcd_put_dc_cs_data(true, 0x0);
    switch (display->line_mode) {
    case 1:
        display_jd9853_write_reg_1line(display, reg);
        break;
    case 2:
        display_jd9853_write_reg_2line(display, reg);
        break;
    case 4:
        display_jd9853_write_reg_4line(display, reg);
        break;
    default:
        display_jd9853_write_reg_1line(display, reg);
        break;
        
    }
}

static FURI_ALWAYS_INLINE void display_jd9853_write_data(DisplayJd9853* display, uint8_t* data, size_t size) {
    for(size_t i = 0; i < size; i++) {
        lcd_put_dc_cs_data(false, data[i]);
    }
    //lcd_put_dc_cs_data(true, 0);
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

    display_jd9853_write_reg(display, caset); // Column address set
    display_jd9853_write_data(display, caset_data, sizeof(caset_data));

    display_jd9853_write_reg(display, paset); // Page address set
    display_jd9853_write_data(display, paset_data, sizeof(paset_data));
}

FURI_ALWAYS_INLINE void display_jd9853_write_buffer_x_y(DisplayJd9853* display, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t* buffer, size_t size) {
    display_jd9853_set_window(display, JD9853_OFF_X + x, JD9853_OFF_Y + y, JD9853_OFF_X + x + (w / 3)-1, JD9853_OFF_Y + y + h - 1);
    display_jd9853_write_reg(display, ramwr);
    display_jd9853_write_data(display, (uint8_t*)buffer, size);
}

FURI_ALWAYS_INLINE void display_jd9853_write_buffer(DisplayJd9853* display, uint16_t w, uint16_t h, const uint8_t* buffer, size_t size) {
    display_jd9853_write_buffer_x_y(display, 0, 0, w, h, buffer, size);
}

void display_jd9853_fill(DisplayJd9853* display, uint8_t color) {
    const size_t width = JD9853_WIDTH; // 1 byte per pixel
    const size_t height = JD9853_HEIGHT;

    // const size_t width = 100; // 1 byte per pixel
    // const size_t height = 100;

    uint8_t* data = (uint8_t*)malloc(width * height);
    for(size_t i = 0; i < width * height; i += 1) {
        data[i] = color;
    }

    display_jd9853_write_buffer(display, width, height, data, width * height);
    // uint16_t x = 100;
    // uint16_t y = 100;
    // display_jd9853_set_window(display, x , y , x+100, y+100);
    // display_jd9853_write_reg(display, ramwr);
    // display_jd9853_write_data(display, (uint8_t*)data, width * height);

    free(data);
}


static void display_jd9853_hstx_init_1_line(void) {
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
}

static void display_jd9853_hstx_init_2_line(void) {
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

    //We have packed 8-bit fields, so shift left 1 bit/cycle, 8 times.
    hstx_ctrl_hw->csr =
        HSTX_CTRL_CSR_EN_BITS |
        (30u << HSTX_CTRL_CSR_SHIFT_LSB) | 
        (4u << HSTX_CTRL_CSR_N_SHIFTS_LSB) |
        (1u << HSTX_CTRL_CSR_CLKDIV_LSB);
}

static void display_jd9853_hstx_init_4_line(void) {
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

        hstx_ctrl_hw->csr =
        HSTX_CTRL_CSR_EN_BITS |
        (28u << HSTX_CTRL_CSR_SHIFT_LSB) |
        (2u << HSTX_CTRL_CSR_N_SHIFTS_LSB) |
        (1u << HSTX_CTRL_CSR_CLKDIV_LSB);
}

DisplayJd9853* display_jd9853_init(void) {
    DisplayJd9853* display = malloc(sizeof(DisplayJd9853));

    
    // Switch HSTX to USB PLL (presumably 48 MHz) because clk_sys is probably
    // running a bit too fast for this example -- 48 MHz means 48 Mbps on
    // PIN_DIN. Need to reset around clock mux change, as the AUX mux can
    // introduce short clock pulses:
    // reset_block(RESETS_RESET_HSTX_BITS);
    // hw_write_masked(
    //     &clocks_hw->clk[clk_hstx].ctrl,
    //     CLOCKS_CLK_HSTX_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB << CLOCKS_CLK_HSTX_CTRL_AUXSRC_LSB,
    //     CLOCKS_CLK_HSTX_CTRL_AUXSRC_BITS
    // );
    // unreset_block_wait(RESETS_RESET_HSTX_BITS);

    clock_configure(clk_hstx,
                        0,
                        CLOCKS_CLK_HSTX_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
                        USB_CLK_HZ,
                        USB_CLK_HZ/4);
    
    
    display->pin_dc = &gpio_display_dc;
    display->pin_reset = &gpio_display_reset;

    //Gpio init
    furi_hal_gpio_init_simple(display->pin_dc, GpioModeOutputPushPull);
    //furi_hal_gpio_init_simple(display->pin_reset, GpioModeOutputOpenDrain);
    furi_hal_gpio_init_simple(display->pin_reset, GpioModeOutputPushPull);
    furi_hal_gpio_write(display->pin_dc, true);

    //Reset display
    //ToDo return to open drain after testing
    // furi_hal_gpio_write_open_drain(display->pin_reset, false);
    // furi_delay_ms(30);
    // furi_hal_gpio_write_open_drain(display->pin_reset, true);
    // furi_delay_ms(30);
    furi_hal_gpio_write(display->pin_reset, false);
    furi_delay_ms(30);
    furi_hal_gpio_write(display->pin_reset, true);
    furi_delay_ms(30);
    
    
    
    //todo set gpio functions add implement furi hal gpio
    gpio_set_function(gpio_display_scl.pin, GPIO_FUNC_HSTX);
    gpio_set_function(gpio_display_sda.pin, GPIO_FUNC_HSTX);
    gpio_set_function(gpio_display_cs.pin, GPIO_FUNC_HSTX);

    gpio_set_function(gpio_display_d0.pin, GPIO_FUNC_HSTX);
    gpio_set_function(gpio_display_d1.pin, GPIO_FUNC_HSTX);
    gpio_set_function(gpio_display_d2.pin, GPIO_FUNC_HSTX);

    //gpio_set_function(gpio_display_dc.pin, GPIO_FUNC_HSTX);   
    
    
    // display->spi_handle = malloc(sizeof(FuriHalSpiHandle));
    // display->spi_handle->id = FuriHalSpiIdSPI0;
    // display->spi_handle->in_use = true;
    // display->pin_dc = &gpio_display_dc;
    // display->pin_reset = &gpio_display_reset;

    // furi_hal_spi_init(display->spi_handle, DISPLAY_BAUNDRATE, FuriHalSpiTransferMode0, FuriHalSpiTransferBitOrderMsbFirst, FuriHalSpiModeMaster);


    display->line_mode = 1; 
    display_jd9853_hstx_init_1_line();
    //Initialization sequence
    display_jd9853_load_config(display, jd9853_init_seq_2025_04_01_normal_black);
    //display_jd9853_load_config(display, jd9853_init_seq_2025_04_01_normal_black);
    display->line_mode = 1; 
    display_jd9853_hstx_init_1_line();

    return display;
}

void display_jd9853_deinit(DisplayJd9853* display) {
    furi_check(display);
    display_jd9853_load_config(display, st7789_deinit_seq);
    furi_hal_gpio_init_ex(display->pin_dc, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_init_ex(display->pin_reset, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    // furi_hal_spi_deinit(display->spi_handle);
    // free(display->spi_handle);
    free(display);
}
