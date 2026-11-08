/* FreeRTOSConfig.h
Copyright 2021 Carl John Kugler III

Licensed under the Apache License, Version 2.0 (the License); you may not use
this file except in compliance with the License. You may obtain a copy of the
License at

   http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software distributed
under the License is distributed on an AS IS BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied. See the License for the
specific language governing permissions and limitations under the License.
*/
/*
 * FreeRTOS V202212.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#if PICO_RP2040
#include "RP2040.h"
#endif
#if PICO_RP2350
#include "RP2350.h"
#endif
//
// #include "my_debug.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef portINLINE
#define portINLINE __inline
#endif
/*-----------------------------------------------------------
 * Application specific definitions.
 *
 * These definitions should be adjusted for your particular hardware and
 * application requirements.
 *----------------------------------------------------------*/

/* Scheduler Related */
#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configUSE_TICKLESS_IDLE                 2
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configCPU_CLOCK_HZ                      SystemCoreClock
#define configTICK_RATE_HZ                      ((TickType_t)1000)
#define configTICK_RATE_HZ_RAW                  1000
#define portTICK_RATE_MS                        ((TickType_t)1000 / configTICK_RATE_HZ)
#define configMAX_PRIORITIES                    32
#define configMINIMAL_STACK_SIZE                (configSTACK_DEPTH_TYPE)128
#define configMAX_TASK_NAME_LEN                 32
//#define configUSE_16_BIT_TICKS                  0
#define configTICK_TYPE_WIDTH_IN_BITS           TICK_TYPE_WIDTH_32_BITS
#define configIDLE_SHOULD_YIELD                 1

/* Synchronization Related */
#define configUSE_TASK_NOTIFICATIONS            1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES   4
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_APPLICATION_TASK_TAG          0
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_ALTERNATIVE_API               0 /* Deprecated! */
//#define configQUEUE_REGISTRY_SIZE               10
#define configQUEUE_REGISTRY_SIZE               0
#define configUSE_QUEUE_SETS                    1
#define configUSE_TIME_SLICING                  1
#define configUSE_NEWLIB_REENTRANT              1 // Necessary if any floating point printfs are used!
#define configENABLE_BACKWARD_COMPATIBILITY     0
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS 5

#define configEXPECTED_IDLE_TIME_BEFORE_SLEEP 4

/* System */
#define configSTACK_DEPTH_TYPE           uint32_t
#define configMESSAGE_BUFFER_LENGTH_TYPE size_t

/* Memory allocation related definitions. */
#define configSUPPORT_STATIC_ALLOCATION  1
#define configSUPPORT_DYNAMIC_ALLOCATION 1
//#define configTOTAL_HEAP_SIZE            (UINT32_MAX)
#define configAPPLICATION_ALLOCATED_HEAP 0

/* Hook function related definitions. */
//#define configCHECK_FOR_STACK_OVERFLOW     2
#define configCHECK_FOR_STACK_OVERFLOW     0
//#define configUSE_MALLOC_FAILED_HOOK       1
#define configUSE_MALLOC_FAILED_HOOK       0
#define configUSE_DAEMON_TASK_STARTUP_HOOK 0

/* Run time and task stats gathering related definitions. */
#define configGENERATE_RUN_TIME_STATS        1
#define configUSE_TRACE_FACILITY             1
#define configUSE_STATS_FORMATTING_FUNCTIONS 1

/* Co-routine related definitions. */
#define configUSE_CO_ROUTINES           0
#define configMAX_CO_ROUTINE_PRIORITIES 1

/* Software timer related definitions. */
#define configUSE_TIMERS          1
//#define configTIMER_TASK_PRIORITY    (configMAX_PRIORITIES - 1)
#define configTIMER_TASK_PRIORITY (2)
//#define configTIMER_QUEUE_LENGTH     10
#define configTIMER_QUEUE_LENGTH  32

//#define configTIMER_TASK_STACK_DEPTH 512
#define configTIMER_TASK_STACK_DEPTH  256
#define configTIMER_SERVICE_TASK_NAME "TimersSrv"

#define configIDLE_TASK_NAME        "(-_-)"
//#define configIDLE_TASK_STACK_DEPTH  512
#define configIDLE_TASK_STACK_DEPTH 256

/* The lowest interrupt priority that can be used in a call to a "set priority"
function. */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 15

/* The highest interrupt priority that can be used by any interrupt service
routine that makes calls to interrupt safe FreeRTOS API functions.  DO NOT CALL
INTERRUPT SAFE FREERTOS API FUNCTIONS FROM ANY INTERRUPT THAT HAS A HIGHER
PRIORITY THAN THIS! (higher priorities are lower numeric values. */
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5

/* Interrupt nesting behaviour configuration.
 *
 * RP2350 is Cortex-M33 with __NVIC_PRIO_BITS = 4, giving 16 hardware levels (0=highest, 15=lowest).
 * The "library" values are the raw 0-15 numbers; the "config" values are left-shifted to fill
 * the 8-bit NVIC priority register field.
 *
 * Encoding: encoded = library_level << (8 - __NVIC_PRIO_BITS) = library_level << 4
 *
 * PendSV / SysTick are set by the port to portMIN_INTERRUPT_PRIORITY (255 -> effective 0xF0 = library 15).
 *
 * basepri in critical sections = configMAX_SYSCALL_INTERRUPT_PRIORITY = 0x50 (library 5).
 *      Interrupts at library 0-4 bypass the kernel (KamiSama zone – no FreeRTOS API allowed).
 *      Interrupts at library 5-15 are blocked inside critical sections and may use FreeRTOS ISR-safe API.
 */
#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - __NVIC_PRIO_BITS))  /* 15<<4 = 0xF0 */

#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - __NVIC_PRIO_BITS))  /* 5<<4 = 0x50 */
/* configMAX_API_CALL_INTERRUPT_PRIORITY is a new name for configMAX_SYSCALL_INTERRUPT_PRIORITY
 that is used by newer ports only. The two are equivalent. */
#define configMAX_API_CALL_INTERRUPT_PRIORITY configMAX_SYSCALL_INTERRUPT_PRIORITY

// /* SMP port only */
// /* https://www.freertos.org/symmetric-multiprocessing-introduction.html */
// #define configNUMBER_OF_CORES         2
// #define configNUM_CORES               configNUMBER_OF_CORES
// #define configTICK_CORE               0
// #define configRUN_MULTIPLE_PRIORITIES 1

// /* SMP Related config. */
// #define configUSE_CORE_AFFINITY     1
// #define configUSE_PASSIVE_IDLE_HOOK 0
// #define portSUPPORT_SMP             1

/* RP2040 specific */
#define configSUPPORT_PICO_SYNC_INTEROP 1
#define configSUPPORT_PICO_TIME_INTEROP 1

// See https://github.com/raspberrypi/FreeRTOS-Kernel/blob/main/portable/ThirdParty/GCC/RP2350_ARM_NTZ/README.md
#define configENABLE_MPU               0
#define configENABLE_TRUSTZONE         0
#define configRUN_FREERTOS_SECURE_ONLY 1
#define configENABLE_FPU               1

/* Define to trap errors during development. */
//#define configASSERT( x )  assert( x )
/* Always-on FreeRTOS assertion: calls vFreeRTOSAssertFailed() declared below */
extern void __attribute__((__noreturn__)) vFreeRTOSAssertFailed(const char *expr, const char *file, int line);
#undef configASSERT
#define configASSERT(__e) \
    do { if(__builtin_expect(!(__e), 0)) { vFreeRTOSAssertFailed(#__e, __FILE__, __LINE__); } } while(0)

/* Set the following definitions to 1 to include the API function, or zero
to exclude the API function. */
#define INCLUDE_vTaskPrioritySet            1
#define INCLUDE_uxTaskPriorityGet           1
#define INCLUDE_vTaskDelete                 1
#define INCLUDE_vTaskSuspend                1
#define INCLUDE_xResumeFromISR              1
#define INCLUDE_vTaskDelayUntil             1
#define INCLUDE_vTaskDelay                  1
#define INCLUDE_xTaskGetSchedulerState      1
#define INCLUDE_xTaskGetCurrentTaskHandle   1
#define INCLUDE_uxTaskGetStackHighWaterMark 1
//#define INCLUDE_xTaskGetIdleTaskHandle      0
#define INCLUDE_xTaskGetIdleTaskHandle      1
//#define INCLUDE_eTaskGetState               0
#define INCLUDE_eTaskGetState               1
#define INCLUDE_xEventGroupSetBitFromISR    1
#define INCLUDE_xTimerPendFunctionCall      1
#define INCLUDE_xTaskAbortDelay             0
#define INCLUDE_xTaskGetHandle              1
#define INCLUDE_xTaskResumeFromISR          1
#define INCLUDE_xQueueGetMutexHolder        1
#define INCLUDE_xSemaphoreGetMutexHolder    1

#define INCLUDE_vTaskCleanUpResources 0

#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS()
extern uint64_t time_us_64(void); // "hardware/timer.h"
#define portGET_RUN_TIME_COUNTER_VALUE() (time_us_64() / 100)

/* A header file that defines trace macro can be included here. */
#define configRECORD_STACK_HIGH_ADDRESS 1

/*
 * The CMSIS-RTOS V2 FreeRTOS wrapper is dependent on the heap implementation used
 * by the application thus the correct define need to be enabled below
 */
#define USE_FreeRTOS_HEAP_4

/* Normal assert() semantics without relying on the provision of an assert.h
header file. header file. Already defined unconditionally above as vFreeRTOSAssertFailed(). */
#if 0
#define configASSERT(x)                \
    if((x) == 0) {                     \
        furi_crash("FreeRTOS Assert"); \
    }
#endif

/* Naked trampoline with .cfi_undefined lr (see furi/core/thread.c) - the
 * unwinder stops at the thread boundary, so no "+ 2" prologue skip needed. */
extern __attribute__((naked, __noreturn__)) void furi_thread_catch(void);
#define configTASK_RETURN_ADDRESS furi_thread_catch

// // Must be last line of config because of recursion
// #include <core/check.h>

#ifdef __cplusplus
}
#endif

#endif /* FREERTOS_CONFIG_H */
