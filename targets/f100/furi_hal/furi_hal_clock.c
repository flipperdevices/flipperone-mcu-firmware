#include <furi_hal_clock.h>
#include <furi_hal_resources.h>
#include <furi.h>

#include <hardware/structs/systick.h>

#define TAG "FuriHalClock"



void furi_hal_clock_init_early(void) {

}

void furi_hal_clock_deinit_early(void) {
}

void furi_hal_clock_init(void) {
    
}

void furi_hal_clock_suspend_tick(void) {
    systick_hw->csr &= ~SysTick_CTRL_ENABLE_Msk;
}

void furi_hal_clock_resume_tick(void) {
    systick_hw->csr |= SysTick_CTRL_ENABLE_Msk;
}


