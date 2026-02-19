#include <furi_hal_clock.h>
#include <furi.h>

#include <hardware/structs/systick.h>
#include <pico/runtime_init.h>
#include <hardware/structs/rosc.h>
#include <hardware/clocks.h>
#include <furi_hal_gpio.h>

#define TAG "FuriHalClock"

static FURI_ALWAYS_INLINE void furi_hal_clock_rosc_write(io_rw_32* addr, uint32_t value) {
    //Clear bad write flag
    hw_clear_bits(&rosc_hw->status, ROSC_STATUS_BADWRITE_BITS);

    furi_check(!(rosc_hw->status & ROSC_STATUS_BADWRITE_BITS));
    *addr = value;
    furi_check(!(rosc_hw->status & ROSC_STATUS_BADWRITE_BITS));
}

void furi_hal_clock_rosc_disable(void) {
    uint32_t tmp = rosc_hw->ctrl;
    tmp &= (~ROSC_CTRL_ENABLE_BITS);
    tmp |= (ROSC_CTRL_ENABLE_VALUE_DISABLE << ROSC_CTRL_ENABLE_LSB);
    furi_hal_clock_rosc_write(&rosc_hw->ctrl, tmp);
    // Wait for stable to go away
    while(rosc_hw->status & ROSC_STATUS_STABLE_BITS)
        ;
}

void furi_hal_clock_rosc_enable(void) {
    //Re-enable the rosc
    furi_hal_clock_rosc_write(&rosc_hw->ctrl, ROSC_CTRL_ENABLE_BITS);

    //Wait for it to become stable once restarted
    while(!(rosc_hw->status & ROSC_STATUS_STABLE_BITS))
        ;
}

void furi_hal_clock_init_early(void) {
}

void furi_hal_clock_deinit_early(void) {
}

FURI_ALWAYS_INLINE void furi_hal_clock_init(void) {
    clocks_init();
}

void furi_hal_clock_suspend_tick(void) {
    systick_hw->csr &= ~SysTick_CTRL_ENABLE_Msk;
}

void furi_hal_clock_resume_tick(void) {
    systick_hw->csr |= SysTick_CTRL_ENABLE_Msk;
}

const uint furi_hal_clock_source[FuriHalClockSourceMax] = {
    [FuriHalClockSourceNone] = 0xFF,
    [FuriHalClockSourcePllSys] = CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS,
    // [FuriHalClockSourceGpin0] = CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLKSRC_GPIN0,
    // [FuriHalClockSourceGpin1] = CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLKSRC_GPIN1,
    [FuriHalClockSourcePllUsb] = CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
    [FuriHalClockSourcePllUsbPrimaryRefOpcg] = CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB_PRIMARY_REF_OPCG,
    [FuriHalClockSourceRosc] = CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_ROSC_CLKSRC,
    [FuriHalClockSourceXosc] = CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_XOSC_CLKSRC,
    [FuriHalClockSourceLposc] = CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_LPOSC_CLKSRC,
    [FuriHalClockSourceSys] = CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLK_SYS,
    [FuriHalClockSourceUsb] = CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLK_USB,
    [FuriHalClockSourceAdc] = CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLK_ADC,
    [FuriHalClockSourceRef] = CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLK_REF,
    [FuriHalClockSourcePeri] = CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLK_PERI,
    [FuriHalClockSourceHstx] = CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLK_HSTX,
    [FuriHalClockSourceOtp2fc] = CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_OTP_CLK2FC,
};

void furi_hal_clock_out_to_gpio13(FuriHalClockSource clk_src, float div) {
    if(clk_src != FuriHalClockSourceNone) {
        clock_gpio_init(13, furi_hal_clock_source[clk_src], div);
    } else {
        GpioPin gpio = {.pin = 13};
        furi_hal_gpio_init_ex(&gpio, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    }
}
