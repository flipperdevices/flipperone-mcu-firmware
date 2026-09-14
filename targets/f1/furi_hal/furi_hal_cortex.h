#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <core/common_defines.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Early init stage for cortex
 */
void furi_hal_cortex_init_early(void);

#ifdef __cplusplus
}
#endif
