/**
 * @file furi_hal_power.h
 * Power HAL API
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void furi_hal_power_reset(void);

/** Reboot into a freshly updated flash partition (flash-update boot).
 *
 * Requests a "flash update" boot: the bootrom boots the partition whose
 * storage offset matches @p partition_storage_offset irrespective of its
 * version, and erases the first sector of the other partition once the
 * image launches. Falls back to a plain reset if the bootrom refuses the
 * request.
 *
 * @param partition_storage_offset flash storage offset of the partition to boot
 */
void furi_hal_power_reboot_to_partition(uint32_t partition_storage_offset);

void furi_hal_power_enter_bootsel(void);

/** Get current insomnia level
 *
 * @return     insomnia level: 0 - no insomnia, >0 - insomnia, bearer count.
 */
uint16_t furi_hal_power_insomnia_level(void);

/** Enter insomnia mode Prevents device from going to sleep
  * @warning    Internally increases insomnia level Must be paired with
  *             furi_hal_power_insomnia_exit
  */
void furi_hal_power_insomnia_enter(void);

/** Exit insomnia mode Allow device to go to sleep
  * @warning    Internally decreases insomnia level. Must be paired with
  *             furi_hal_power_insomnia_enter
  */
void furi_hal_power_insomnia_exit(void);

/** Check if sleep available
  *
  * @return     true if available
  */
bool furi_hal_power_sleep_available(void);

/** Initialize power HAL
  */
void furi_hal_power_init(void);

/** Enter deep sleep mode
  *
  * @param expected_idle_ticks Expected idle ticks
  * @return Number of ticks spent in deep sleep
  */
uint32_t furi_hal_power_deep_sleep(uint32_t expected_idle_ticks);

#ifdef __cplusplus
}
#endif
