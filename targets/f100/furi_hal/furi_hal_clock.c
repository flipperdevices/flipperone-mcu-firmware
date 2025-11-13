#include <furi_hal_clock.h>
#include <furi.h>

#include <hardware/structs/systick.h>
#include <pico/runtime_init.h>
#include <hardware/structs/rosc.h>

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
