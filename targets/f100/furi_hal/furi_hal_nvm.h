#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FuriHalRtcBootModeDummy, //Just to get it to compile
} FuriHalNvmBootMode;

/** Get RTC boot mode
 *
 * @return     The RTC boot mode.
 */
FuriHalNvmBootMode furi_hal_nvm_get_boot_mode(void);

/** Store fault data
 *
 * @return     The fault data.
 */
void furi_hal_nvm_set_fault_data(uint32_t value);

#ifdef __cplusplus
}
#endif
