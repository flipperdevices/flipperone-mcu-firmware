#pragma once

#include <furi.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "blockdevice/blockdevice.h"

blockdevice_t* furi_hal_nvm_storage_init(void);

void furi_hal_nvm_storage_deinit(blockdevice_t* device);

void furi_hal_nvm_storage_wipe(blockdevice_t* device);

#ifdef __cplusplus
}
#endif
