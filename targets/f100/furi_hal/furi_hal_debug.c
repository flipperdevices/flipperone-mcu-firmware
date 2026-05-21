#include <furi_hal_debug.h>
#include <furi_hal_resources.h>

#define TAG "FuriHalDebug"

//todo: implement detection of gdb session
// deep sleep mode OFF
//volatile bool furi_hal_debug_gdb_session_active = false;
volatile bool furi_hal_debug_gdb_session_active = true;

void furi_hal_debug_enable(void) {
    // Low power mode debug
    //TODO: add implementation
}

void furi_hal_debug_disable(void) {
    // Low power mode debug
    //TODO: add implementation
}

bool furi_hal_debug_is_gdb_session_active(void) {
    return furi_hal_debug_gdb_session_active;
}


#include <stdint.h>
#include <inttypes.h>
#include "furi.h"

__attribute__((naked)) void isr_hardfault(void) {
    __asm volatile(
        "tst lr, #4                \n" // test EXC_RETURN bit 2 to decide stack pointer
        "ite eq                    \n"
        "mrseq r0, msp             \n" // r0 = MSP if equal
        "mrsne r0, psp             \n" // r0 = PSP otherwise
        "mov r1, lr                \n" // r1 = lr
        "b hardfault_handler_c     \n"
    );
}

void hardfault_handler_c(uint32_t *stack_frame, uint32_t lr_value) {
    uint32_t r0    = stack_frame[0];
    uint32_t r1    = stack_frame[1];
    uint32_t r2    = stack_frame[2];
    uint32_t r3    = stack_frame[3];
    uint32_t r12   = stack_frame[4];
    uint32_t lr_st = stack_frame[5];
    uint32_t pc    = stack_frame[6];
    uint32_t psr   = stack_frame[7];

    FURI_LOG_E(TAG, "HardFault! lr=0x%08" PRIX32 " lr_st=0x%08" PRIX32 " pc=0x%08" PRIX32 " psr=0x%08" PRIX32,
               (uint32_t)lr_value, (uint32_t)lr_st, (uint32_t)pc, (uint32_t)psr);
    FURI_LOG_E(TAG, "R0=0x%08" PRIX32 " R1=0x%08" PRIX32 " R2=0x%08" PRIX32 " R3=0x%08" PRIX32,
               (uint32_t)r0, (uint32_t)r1, (uint32_t)r2, (uint32_t)r3);
    FURI_LOG_E(TAG, "R12=0x%08" PRIX32, (uint32_t)r12);

    volatile uint32_t cfsr  = (*(volatile uint32_t*)0xE000ED28);
    volatile uint32_t hfsr  = (*(volatile uint32_t*)0xE000ED2C);
    volatile uint32_t dfsr  = (*(volatile uint32_t*)0xE000ED30);
    volatile uint32_t mmfar = (*(volatile uint32_t*)0xE000ED34);
    volatile uint32_t bfar  = (*(volatile uint32_t*)0xE000ED38);

    FURI_LOG_E(TAG, "CFSR=0x%08" PRIX32 " HFSR=0x%08" PRIX32 " DFSR=0x%08" PRIX32 " MMFAR=0x%08" PRIX32 " BFAR=0x%08" PRIX32,
               (uint32_t)cfsr, (uint32_t)hfsr, (uint32_t)dfsr, (uint32_t)mmfar, (uint32_t)bfar);

    // Attempt to hand off to platform crash handler (if available)
    furi_crash("HardFault");

    // If furi_crash returns, halt for debugger
    while(1) __asm volatile("bkpt #0");
}