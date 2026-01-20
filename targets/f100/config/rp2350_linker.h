/**
 * @file stm32wb55_linker.h
 *
 * Linker defined symbols. Used in various part of firmware to understand
 * hardware boundaries.
 * 
 */
#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const void _stack_end; /**< end of stack */
extern const void _stack_size; /**< stack size */

extern const void _sidata; /**< data initial value start */
extern const void _sdata; /**< data start */
extern const void _edata; /**< data end */

extern const void _sbss; /**< bss start */
extern const void _ebss; /**< bss end */

extern const void __heap_start; /**< RAM1 Heap start */
extern const void __heap_end; /**< RAM1 Heap end */
//extern const void __heap_size__; /**< RAM1 Heap size (as symbol address) */

extern const void __free_flash_start__; /**< Free Flash space start */

extern const void __bkp_start__;
extern const void __bkp_end__;
extern const void __bkp_size__;

#ifdef __cplusplus
}
#endif
