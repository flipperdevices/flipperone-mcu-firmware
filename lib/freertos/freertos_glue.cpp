#include "FreeRTOS.h"
#include "task.h"
#include "drivers/log.hpp"

#if (configCHECK_FOR_STACK_OVERFLOW > 0)

extern "C" void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName) {
    Log::error("Stack overflow in task: %s[%p]", pcTaskName, xTask);
    while(1) {
        /* Loop forever */
    }
}

extern "C" void vApplicationMallocFailedHook(void) {
    Log::error("Memory allocation failed!");
    while(1) {
        /* Loop forever */
    }
}

#endif
