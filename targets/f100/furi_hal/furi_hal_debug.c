#include <furi_hal_debug.h>
#include <furi_hal_resources.h>
#include <furi.h>
#include <FreeRTOS.h>
#include <task.h>

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


__attribute__((naked)) void isr_hardfault(void) {
    __asm volatile(
        /* IMPORTANT: Read the fault stack pointer BEFORE push, otherwise
         * mrseq r0, msp would return the post-push (offset) value. */
        "tst lr, #4                \n" /* EXC_RETURN bit[2]: 0=MSP, 1=PSP */
        "ite eq                    \n"
        "mrseq r0, msp             \n" /* r0 = hardware exception frame (MSP) */
        "mrsne r0, psp             \n" /* r0 = hardware exception frame (PSP) */
        "mov r1, lr                \n" /* r1 = EXC_RETURN */
        "stmdb sp!, {r4-r11}       \n" /* NOW save r4-r11 (uses MSP as sp) */
        "mov r2, sp                \n" /* r2 = pointer to saved r4-r11 */
        "b hardfault_handler_c     \n"
    );
}

void hardfault_handler_c(uint32_t *stack_frame, uint32_t lr_value, uint32_t *saved_regs) {
    /* Hardware-saved frame: r0, r1, r2, r3, r12, lr, pc, xpsr */
    uint32_t r0    = stack_frame[0];
    uint32_t r1    = stack_frame[1];
    uint32_t r2    = stack_frame[2];
    uint32_t r3    = stack_frame[3];
    uint32_t r12   = stack_frame[4];
    uint32_t lr_st = stack_frame[5];
    uint32_t pc    = stack_frame[6];
    uint32_t psr   = stack_frame[7];

    /* Manually saved r4-r11 from the faulting context */
    uint32_t r4  = saved_regs[0];
    uint32_t r5  = saved_regs[1];
    uint32_t r6  = saved_regs[2];
    uint32_t r7  = saved_regs[3];
    uint32_t r8  = saved_regs[4];
    uint32_t r9  = saved_regs[5];
    uint32_t r10 = saved_regs[6];
    uint32_t r11 = saved_regs[7];

    /* Stack limit registers */
    uint32_t msplim, psplim;
    __asm volatile("mrs %0, msplim" : "=r"(msplim));
    __asm volatile("mrs %0, psplim" : "=r"(psplim));

    /* PSP — task stack at the moment of fault (valid even if fault is in handler) */
    uint32_t psp_val;
    __asm volatile("mrs %0, psp" : "=r"(psp_val));

    FURI_LOG_E(TAG, "HardFault! lr=0x%08lX lr_st=0x%08lX pc=0x%08lX psr=0x%08lX",
               lr_value, lr_st, pc, psr);
    FURI_LOG_E(TAG, "R0=0x%08lX R1=0x%08lX R2=0x%08lX R3=0x%08lX",
               r0, r1, r2, r3);
    FURI_LOG_E(TAG, "R4=0x%08lX R5=0x%08lX R6=0x%08lX R7=0x%08lX",
               r4, r5, r6, r7);
    FURI_LOG_E(TAG, "R8=0x%08lX R9=0x%08lX R10=0x%08lX R11=0x%08lX R12=0x%08lX",
               r8, r9, r10, r11, r12);

    volatile uint32_t cfsr  = *(volatile uint32_t*)0xE000ED28;
    volatile uint32_t hfsr  = *(volatile uint32_t*)0xE000ED2C;
    volatile uint32_t dfsr  = *(volatile uint32_t*)0xE000ED30;
    volatile uint32_t mmfar = *(volatile uint32_t*)0xE000ED34;
    volatile uint32_t bfar  = *(volatile uint32_t*)0xE000ED38;

    FURI_LOG_E(TAG, "CFSR=0x%08lX HFSR=0x%08lX DFSR=0x%08lX MMFAR=0x%08lX BFAR=0x%08lX",
               cfsr, hfsr, dfsr, mmfar, bfar);
    FURI_LOG_E(TAG, "MSPLIM=0x%08lX PSPLIM=0x%08lX PSP=0x%08lX",
               msplim, psplim, psp_val);

    /* If fault is from handler mode (bit2=0), PSP still has the task frame */
    if((lr_value & 0x4) == 0 && psp_val > 0x20000000u && psp_val < 0x20080000u) {
        uint32_t *task_frame = (uint32_t*)psp_val;
        FURI_LOG_E(TAG, "Task frame (PSP): PC=0x%08lX LR=0x%08lX PSR=0x%08lX",
                   task_frame[6], task_frame[5], task_frame[7]);
    }

    /* Current FreeRTOS task name via Furi API (this is the OUTGOING task) */
    FuriThreadId current = furi_thread_get_current_id();
    if(current) {
        FURI_LOG_E(TAG, "FreeRTOS task (outgoing): %s", furi_thread_get_name(current));
    }

    /* Dump stack watermark for ALL tasks to find the one with near-zero free stack.
     * IMPORTANT: we are in ISR — malloc is forbidden.
     * Use uxTaskGetSystemState with a static TaskStatus_t array — no allocation needed. */
    FURI_LOG_E(TAG, "--- Task stack watermarks ---");
    {
        /* Static array: enough for up to 32 tasks. Placed in .bss — no stack cost per crash. */
        static TaskStatus_t task_status_buf[32];
        UBaseType_t task_count = uxTaskGetSystemState(
            task_status_buf, sizeof(task_status_buf) / sizeof(task_status_buf[0]), NULL);
        for(UBaseType_t i = 0; i < task_count; i++) {
            /* usStackHighWaterMark is in words (4 bytes each) */
            uint32_t free_bytes = (uint32_t)task_status_buf[i].usStackHighWaterMark * 4u;
            const char* low_mark = (free_bytes < 512u) ? "  *** LOW ***" : "";
            FURI_LOG_E(TAG, "  %s %-20s %4lu bytes free%s",
                (free_bytes < 512u) ? "[!]" : "[ ]",
                task_status_buf[i].pcTaskName, free_bytes, low_mark);
        }
    }
    FURI_LOG_E(TAG, "--- end ---");

    furi_crash("HardFault");
    while(1) __asm volatile("bkpt #0");
}