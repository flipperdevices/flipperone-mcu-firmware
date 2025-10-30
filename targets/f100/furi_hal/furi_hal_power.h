/**
 * @file furi_hal_power.h
 * Power HAL API
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void furi_hal_power_reset(void);

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

#ifdef __cplusplus
}
#endif
