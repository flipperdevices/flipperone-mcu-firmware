/**
 * @file furi_hal_cpu.h
 * @brief CPU information API.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get the CPU frequency, in Hertz.
 *
 * @returns CPU frequency in Hz
 */
uint32_t furi_hal_cpu_get_frequency(void);

/**
 * @brief Get the current CPU cycles counter value.
 *
 * The counter value is monotonic and may wrap.
 *
 * @returns CPU cycles counter value
 */
uint32_t furi_hal_cpu_get_cycle_count(void);

/**
 * @brief Get the number of CPU cycles per microsecond.
 *
 * @returns The number of CPU cycles per microsecond
 */
uint32_t furi_hal_cpu_get_cycles_per_us(void);

#ifdef __cplusplus
}
#endif
