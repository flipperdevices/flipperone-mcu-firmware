#include "furi_hal_nvm.h"

#include <core/core_defines.h>

FuriHalNvmBootMode furi_hal_nvm_get_boot_mode(void) {
    return FuriHalRtcBootModeDummy;
}

void furi_hal_nvm_set_fault_data(uint32_t value) {
    UNUSED(value);
    /* This function does nothing */
}
