#include "furi_hal_flash.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <core/common_defines.h>
#include <furi.h>
#include <hardware/flash.h>

#include "pico/bootrom.h"
#include "boot/picobin.h"

bool furi_hal_flash_get_partition_info(FlashPartitionId partition_id, size_t* base, size_t* size) {
    // See datasheet 5.4.8.16; 5.9.4.2 for more details
    uint32_t buffer[(3 * 4) + 1] = {0}; // maximum of 3 partitions, each with maximum of 4 words returned, plus 1 for header
    int ret = rom_get_partition_table_info(buffer, COUNT_OF(buffer), PT_INFO_PARTITION_LOCATION_AND_FLAGS | PT_INFO_PARTITION_ID);

    furi_check(buffer[0] == (PT_INFO_PARTITION_LOCATION_AND_FLAGS | PT_INFO_PARTITION_ID));

    uint32_t partition_count = (ret - 1) / 4;
    for(size_t i = 0; i < partition_count; i++) {
        size_t offset = 1 + i * 4;

        uint32_t id = buffer[offset + 2];
        if(id == partition_id) {
            uint32_t start = (buffer[offset] & PICOBIN_PARTITION_LOCATION_FIRST_SECTOR_BITS) >> PICOBIN_PARTITION_LOCATION_FIRST_SECTOR_LSB;
            uint32_t end = (buffer[offset] & PICOBIN_PARTITION_LOCATION_LAST_SECTOR_BITS) >> PICOBIN_PARTITION_LOCATION_LAST_SECTOR_LSB;
            if(size) *size = (end - start + 1) * 4096;
            if(base) *base = start * 4096;
            return true;
        }
    }
    return false;
}

FlashPartitionId furi_hal_flash_get_active_fw_partition(void) {
    boot_info_t boot_info = {};
    int ret = rom_get_boot_info(&boot_info);
    furi_check(ret);
    return boot_info.partition == 0 ? FlashPartitionIdFwA : FlashPartitionIdFwB;
}

bool furi_hal_flash_rollback(void) {
    FlashPartitionId active_partition = furi_hal_flash_get_active_fw_partition();
    // TODO: manually check if there is a valid firmware in opposite partition
    size_t part_base;
    bool ret = furi_hal_flash_get_partition_info(active_partition, &part_base, NULL);
    furi_check(ret);
    FURI_CRITICAL_ENTER();
    flash_range_erase(part_base, FLASH_SECTOR_SIZE);
    FURI_CRITICAL_EXIT();

    return true;
}

size_t furi_hal_flash_get_page_size(void) {
    return FLASH_PAGE_SIZE;
}

size_t furi_hal_flash_get_base(void) {
    // Always return the base of the first bank (abstract both banks as a single region)
    return XIP_BASE;
    // FIXME: get from partition table
}

const void* furi_hal_flash_get_free_end_address(void) {
    // The end of the free region is the end of the flash (abstract both banks as a single region)
    return (const void*)(XIP_BASE + PICO_FLASH_SIZE_BYTES);
    // FIXME: get from partition table
}
