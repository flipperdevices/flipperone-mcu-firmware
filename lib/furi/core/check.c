#include "check.h"
#include "log.h"
#include "common_defines.h"

#include <furi_hal_debug.h>
#include <furi_hal_interrupt.h>
#include <furi_hal_power.h>
#include <furi_hal_nvm.h>
#include <furi_hal_flash.h>

#include <stdio.h>

#include <FreeRTOS.h>
#include <task.h>
#include <stdio.h>
#include <stdlib.h>

static const char* volatile __furi_check_message = NULL;
static volatile uint32_t __furi_check_registers[13] = {0};

/** Load r12 value to __furi_check_message and store registers to __furi_check_registers */
#define GET_MESSAGE_AND_STORE_REGISTERS()               \
    asm volatile("ldr r11, =__furi_check_message    \n" \
                 "str r12, [r11]                    \n" \
                 "ldr r12, =__furi_check_registers  \n" \
                 "stm r12, {r0-r11}                 \n" \
                 "str lr, [r12, #48]                \n" \
                 :                                      \
                 :                                      \
                 : "memory");

/** Restore registers and halt MCU
 * 
 * - Always use it with GET_MESSAGE_AND_STORE_REGISTERS
 * - If debugger is(was) connected this routine will raise bkpt
 * - If debugger is not connected then endless loop
 * 
 */
#define RESTORE_REGISTERS_AND_HALT_MCU(debug)           \
    register bool r0 asm("r0") = debug;                 \
    asm volatile("cbnz  r0, with_debugger%=         \n" \
                 "ldr   r12, =__furi_check_registers\n" \
                 "ldm   r12, {r0-r11}               \n" \
                 "loop%=:                           \n" \
                 "wfi                               \n" \
                 "b     loop%=                      \n" \
                 "with_debugger%=:                  \n" \
                 "ldr   r12, =__furi_check_registers\n" \
                 "ldm   r12, {r0-r11}               \n" \
                 "debug_loop%=:                     \n" \
                 "bkpt  0x00                        \n" \
                 "wfi                               \n" \
                 "b     debug_loop%=                \n" \
                 :                                      \
                 : "r"(r0)                              \
                 : "memory");

extern size_t xPortGetTotalHeapSize(void);

static void __furi_print_register_info(void) {
    // Print registers
    for(uint8_t i = 0; i < 12; i++) {
        furi_log_puts("\r\n\tr");
        furi_log_putu32(i);
        furi_log_puts(" : ");
        furi_log_puthex32(__furi_check_registers[i]);
    }

    furi_log_puts("\r\n\tlr : ");
    furi_log_puthex32(__furi_check_registers[12]);
}

static void __furi_print_stack_info(void) {
    furi_log_puts("\r\n\tstack watermark: ");
    furi_log_putu32(uxTaskGetStackHighWaterMark(NULL) * 4);
}

static void __furi_print_heap_info(void) {
    furi_log_puts("\r\n\t     heap total: ");
    furi_log_putu32(xPortGetTotalHeapSize());
    furi_log_puts("\r\n\t      heap free: ");
    furi_log_putu32(xPortGetFreeHeapSize());
    furi_log_puts("\r\n\t heap watermark: ");
    furi_log_putu32(xPortGetMinimumEverFreeHeapSize());
    furi_log_puts("\r\n\n");
}

static void __furi_print_name(bool isr) {
    if(isr) {
        uint8_t exception_number = __get_IPSR();
        const char* name = furi_hal_interrupt_get_name(exception_number);
        furi_log_puts("[ISR ");
        if(name) {
            furi_log_puts(name);
        } else {
            furi_log_putu32(__get_IPSR());
        }
        furi_log_puts("] ");
    } else {
        const char* name = pcTaskGetName(NULL);
        if(name == NULL) {
            furi_log_puts("[main] ");
        } else {
            furi_log_puts("[");
            furi_log_puts(name);
            furi_log_puts("] ");
        }
    }
}

FURI_WEAK void furi_crash_handler() {
    furi_log_puts("No additional crash handler defined.\r\n");
}

FURI_NORETURN void __furi_crash_implementation(void) {
    __disable_irq();
    GET_MESSAGE_AND_STORE_REGISTERS();

    bool isr = FURI_IS_IRQ_MODE();

    if(__furi_check_message == NULL) {
        __furi_check_message = "Fatal Error";
    } else if(__furi_check_message == (void*)__FURI_ASSERT_MESSAGE_FLAG) {
        __furi_check_message = "furi_assert failed";
    } else if(__furi_check_message == (void*)__FURI_CHECK_MESSAGE_FLAG) {
        __furi_check_message = "furi_check failed";
    }

    furi_log_puts("\r\n\033[0;31m[CRASH]");
    __furi_print_name(isr);
    furi_log_puts(__furi_check_message);

    __furi_print_register_info();
    if(!isr) {
        __furi_print_stack_info();
    }
    __furi_print_heap_info();

    furi_crash_handler();

    // Check if debug enabled by DAP
    // https://developer.arm.com/documentation/ddi0403/d/Debug-Architecture/ARMv7-M-Debug/Debug-register-support-in-the-SCS/Debug-Halting-Control-and-Status-Register--DHCSR?lang=en
    bool debug = CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk;
#ifndef FURI_DEBUG
    if(debug) {
#endif
        furi_log_puts("\r\nSystem halted. Connect debugger for more info\r\n");
        furi_log_puts("\033[0m\r\n");
        furi_hal_debug_enable();

        RESTORE_REGISTERS_AND_HALT_MCU(debug);
#ifndef FURI_DEBUG
    } else {
        uint32_t ptr = (uint32_t)__furi_check_message;
        if(ptr < (uint32_t)furi_hal_flash_get_base() ||
           ptr > (uint32_t)furi_hal_flash_get_free_end_address()) {
            ptr = (uint32_t) "Check serial logs";
        }
        furi_hal_nvm_set_fault_data(ptr);
        furi_log_puts("\r\nRebooting system.\r\n");
        furi_log_puts("\033[0m\r\n");
        furi_hal_power_reset();
    }
#endif
    __builtin_unreachable();
}

FURI_NORETURN void __furi_halt_implementation(void) {
    __disable_irq();
    GET_MESSAGE_AND_STORE_REGISTERS();

    bool isr = FURI_IS_IRQ_MODE();

    if(__furi_check_message == NULL) {
        __furi_check_message = "System halt requested.";
    }

    furi_log_puts("\r\n\033[0;31m[HALT]");
    __furi_print_name(isr);
    furi_log_puts(__furi_check_message);
    furi_log_puts("\r\nSystem halted. Bye-bye!\r\n");
    furi_log_puts("\033[0m\r\n");

    // Check if debug enabled by DAP
    // https://developer.arm.com/documentation/ddi0403/d/Debug-Architecture/ARMv7-M-Debug/Debug-register-support-in-the-SCS/Debug-Halting-Control-and-Status-Register--DHCSR?lang=en
    bool debug = CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk;
    furi_log_putu32(debug);

    __builtin_unreachable();
}
