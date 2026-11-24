#include "FreeRTOS.h"

/* Bottom frame of every task: configTASK_RETURN_ADDRESS, their initial LR, points in here.
 * Its unwind info marks the return address undefined, so debuggers end task backtraces here. */
__attribute__((naked, __noreturn__)) void freertos_task_return(void) {
    asm volatile(
#ifdef __GCC_HAVE_DWARF2_CFI_ASM
        ".cfi_undefined lr\n"
#endif
        /* Debuggers look up a caller frame at its return address minus one, so
         * configTASK_RETURN_ADDRESS points past this nop to keep that lookup in here */
        "nop.n\n"
        "b furi_thread_catch\n");
}
