#include "FreeRTOS.h"
#include "task.h"
//#include "drivers/log.hpp"

extern "C" void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName) {
    //Log::error("Stack overflow in task: %s[%p]", pcTaskName, xTask);
    while(1) {
        /* Loop forever */
    }
}

extern "C" void vApplicationMallocFailedHook(void) {
    //Log::error("Memory allocation failed!");
    while(1) {
        /* Loop forever */
    }
}

// extern "C" void vApplicationGetIdleTaskMemory(StaticTask_t** tcb_ptr, StackType_t** stack_ptr, uint32_t* stack_size) {
//     static StaticTask_t task_tcb;
//     static StackType_t task_stack[configIDLE_TASK_STACK_DEPTH];
//     *tcb_ptr = &task_tcb;
//     *stack_ptr = task_stack;
//     *stack_size = configIDLE_TASK_STACK_DEPTH;
// }

// extern "C" void vApplicationGetTimerTaskMemory(StaticTask_t** tcb_ptr, StackType_t** stack_ptr, uint32_t* stack_size) {
//     static StaticTask_t task_tcb;
//     static StackType_t task_stack[configTIMER_TASK_STACK_DEPTH];
//     *tcb_ptr = &task_tcb;
//     *stack_ptr = task_stack;
//     *stack_size = configTIMER_TASK_STACK_DEPTH;
// }

extern "C" void vApplicationGetPassiveIdleTaskMemory(StaticTask_t** tcb_ptr, StackType_t** stack_ptr, uint32_t* stack_size, BaseType_t core_id) {
    static StaticTask_t task_tcb[configNUMBER_OF_CORES - 1];
    static StackType_t task_stack[configNUMBER_OF_CORES - 1][configIDLE_TASK_STACK_DEPTH];
    *tcb_ptr = &task_tcb[core_id];
    *stack_ptr = task_stack[core_id];
    *stack_size = configIDLE_TASK_STACK_DEPTH;
}
