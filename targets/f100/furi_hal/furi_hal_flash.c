#include "furi_hal_flash.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <core/common_defines.h>
#include <furi.h>
#include <hardware/flash.h>

size_t furi_hal_flash_get_page_size(void) {
    return FLASH_PAGE_SIZE;
}

size_t furi_hal_flash_get_base(void) {
    // Always return the base of the first bank (abstract both banks as a single region)
    return XIP_BASE;
}

const void* furi_hal_flash_get_free_end_address(void) {
    // The end of the free region is the end of the flash (abstract both banks as a single region)
    return (const void*)(XIP_BASE + PICO_FLASH_SIZE_BYTES);
}