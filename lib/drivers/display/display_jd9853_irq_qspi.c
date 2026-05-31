#include "display_jd9853_qspi.h"
#include "display_jd9853_reg.h"

#include <furi_hal_gpio.h>
#include <furi_hal_resources.h>

#include <hardware/structs/clocks.h>
#include <hardware/structs/hstx_ctrl.h>
#include <hardware/structs/hstx_fifo.h>
#include <pico/types.h>

#include <hardware/clocks.h>

#define FIRST_HSTX_PIN 12

static FURI_ALWAYS_INLINE void display_jd9853_irq_hstx_wait_complete(void) {
    while(!(hstx_fifo_hw->stat & HSTX_FIFO_STAT_EMPTY_BITS)) {
        tight_loop_contents();
    }
}

static FURI_ALWAYS_INLINE void display_jd9853_irq_hstx_init_1_line(void) {
    display_jd9853_irq_hstx_wait_complete();

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

static FURI_ALWAYS_INLINE void display_jd9853_irq_hstx_init_4_line(void) {
    display_jd9853_irq_hstx_wait_complete();

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

static FURI_ALWAYS_INLINE void display_jd9853_irq_hstx_put_word(uint32_t data) {
    while(hstx_fifo_hw->stat & HSTX_FIFO_STAT_FULL_BITS) {
        tight_loop_contents();
    }
    hstx_fifo_hw->fifo = data;
}

static FURI_ALWAYS_INLINE void display_jd9853_irq_cs_up(void) {
    display_jd9853_irq_hstx_put_word(0x0ff00000u);
}

static FURI_ALWAYS_INLINE void display_jd9853_irq_write_reg_1line(DisplayJd9853Reg reg) {
    display_jd9853_irq_hstx_put_word(JD9853_QSPI_CMD_1_LINE_MODE); // Command Write Quad SPI
    display_jd9853_irq_hstx_put_word((uint8_t)0x00);
    display_jd9853_irq_hstx_put_word((uint8_t)reg);
    display_jd9853_irq_hstx_put_word((uint8_t)0x00);
}

static FURI_ALWAYS_INLINE void display_jd9853_irq_write_reg(DisplayJd9853Reg reg) {
    display_jd9853_irq_cs_up();
    display_jd9853_irq_write_reg_1line(reg);
}

static FURI_ALWAYS_INLINE void display_jd9853_irq_write_data(uint8_t* data, size_t size) {
    for(size_t i = 0; i < size; i++) {
        display_jd9853_irq_hstx_put_word(data[i]);
    }
    display_jd9853_irq_cs_up();
}

static void display_jd9853_irq_load_config(const uint8_t* config) {
    while(*config) {
        display_jd9853_irq_write_reg((DisplayJd9853Reg)(*(config + 2)));

        if(*(config)) {
            display_jd9853_irq_write_data((uint8_t*)(config + 3), *(config)-1);
        }
        furi_delay_ms(*(config + 1) * 5);
        config += *(config) + 2;
    }
}

static void display_jd9853_irq_hstx_clock_init(void) {
    clock_configure(clk_hstx, 0, CLOCKS_CLK_HSTX_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB, USB_CLK_HZ, USB_CLK_HZ);
}

void display_jd9853_irq_qspi_on_sleep_exit(void) {
    display_jd9853_irq_hstx_clock_init();
}

static void display_jd9853_irq_qspi_init(void) {
    // Configure HSTX clock
    display_jd9853_irq_hstx_clock_init();

    //Gpio init
    furi_hal_gpio_init_simple(&gpio_display_reset, GpioModeOutputOpenDrain);
    furi_hal_gpio_init_simple(&gpio_display_cs, GpioModeOutputPushPull);
    furi_hal_gpio_write(&gpio_display_cs, true);

    //Reset display
    furi_hal_gpio_write_open_drain(&gpio_display_reset, false);
    furi_delay_ms(30);
    furi_hal_gpio_write_open_drain(&gpio_display_reset, true);
    furi_delay_ms(30);

    //todo set gpio functions add implement furi hal gpio
    furi_hal_gpio_set_function(&gpio_display_scl, GpioAltFn0Hstx);
    furi_hal_gpio_set_function(&gpio_display_sda, GpioAltFn0Hstx);
    furi_hal_gpio_set_function(&gpio_display_cs, GpioAltFn0Hstx);
    furi_hal_gpio_set_function(&gpio_display_d0, GpioAltFn0Hstx);
    furi_hal_gpio_set_function(&gpio_display_d1, GpioAltFn0Hstx);
    furi_hal_gpio_set_function(&gpio_display_d2, GpioAltFn0Hstx);

    display_jd9853_irq_hstx_init_1_line();

    //Initialization sequence
    display_jd9853_irq_load_config(jd9853_init_seq_2025_04_01_normal_white_mod);
}

void display_jd9853_irq_qspi_deinit(void) {
    display_jd9853_irq_load_config(jd9853_deinit_seq);
    furi_hal_gpio_init_ex(&gpio_display_reset, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_init_ex(&gpio_display_cs, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_init_ex(&gpio_display_sda, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_init_ex(&gpio_display_scl, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_init_ex(&gpio_display_d0, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_init_ex(&gpio_display_d1, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_init_ex(&gpio_display_d2, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);

    clock_stop(clk_hstx);
}

void display_jd9853_irq_qspi_write_buffer(const uint8_t* buffer, size_t size) {
    furi_check(size == JD9853_WIDTH * JD9853_HEIGHT); //size must be equal to full buffer size

    if(!display_jd9853_qspi_is_init()) {
        display_jd9853_irq_qspi_init();
    }

    display_jd9853_irq_hstx_init_1_line();
    display_jd9853_irq_write_reg(ramwr); // Memory write
    display_jd9853_irq_write_data((uint8_t*)buffer, size);
    display_jd9853_irq_hstx_init_4_line();
}