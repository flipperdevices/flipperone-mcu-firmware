#include "furi_hal_flash.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <core/common_defines.h>
#include <furi.h>
#include <hardware/flash.h>

#include "pico/bootrom.h"
#include "boot/picobin.h"
#include "boot/picoboot_constants.h"

static uint8_t* find_picobin_block(const uint8_t* start, size_t size) {
    for(size_t offset = 0; offset < size; offset++) {
        const uint8_t* ptr = start + offset;
        if(*(uint32_t*)ptr == PICOBIN_BLOCK_MARKER_START) {
            return (uint8_t*)ptr;
        }
    }
    return NULL;
}

static uint8_t* find_picobin_item(const uint8_t* picobin_ptr, size_t picobin_size, uint8_t wanted_item_type, size_t* out_item_size) {
    size_t offset = 4;
    while(offset < picobin_size) {
        if(*(uint32_t*)&picobin_ptr[offset] == PICOBIN_BLOCK_MARKER_END) break;

        uint8_t item_type = picobin_ptr[offset];
        size_t item_size = 0;
        if((item_type & 0x80) == 0) {
            item_size = picobin_ptr[offset + 1];
        } else {
            item_size = *(uint16_t*)&picobin_ptr[offset + 1];
        }

        if(item_size == 0) break;

        if(item_type == wanted_item_type) {
            if(out_item_size) *out_item_size = item_size;
            return (uint8_t*)&picobin_ptr[offset];
        }

        if(item_type == PICOBIN_BLOCK_ITEM_2BS_LAST) break;
        offset += item_size * 4;
    }

    return NULL;
}

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

bool furi_hal_flash_fw_partition_is_valid(FlashPartitionId partition_id) {
    size_t part_base;
    bool ret = furi_hal_flash_get_partition_info(partition_id, &part_base, NULL);
    furi_check(ret);

    const uint8_t* start_ptr = furi_hal_flash_get_read_ptr(part_base);
    uint8_t* picobin_ptr = find_picobin_block(start_ptr, 4096);

    return (picobin_ptr != NULL);
}

bool furi_hal_flash_fw_partition_invalidate(FlashPartitionId partition_id) {
    size_t part_base;
    bool ret = furi_hal_flash_get_partition_info(partition_id, &part_base, NULL);
    furi_check(ret);

    const uint8_t* start_ptr = furi_hal_flash_get_read_ptr(part_base);
    uint8_t* picobin_ptr = find_picobin_block(start_ptr, 4096);
    if(!picobin_ptr) return false;

    size_t offset = picobin_ptr - start_ptr;
    size_t page_nb = offset / FLASH_PAGE_SIZE;
    size_t page_offset = offset % FLASH_PAGE_SIZE;

    uint8_t* page_buf = malloc(FLASH_PAGE_SIZE);
    furi_hal_flash_read(part_base + page_nb * FLASH_PAGE_SIZE, page_buf, FLASH_PAGE_SIZE);
    memset(&page_buf[page_offset], 0, 4);
    furi_hal_flash_write(part_base + page_nb * FLASH_PAGE_SIZE, page_buf, FLASH_PAGE_SIZE);
    free(page_buf);

    return true;
}

bool furi_hal_flash_get_image_version(const uint8_t* start, uint16_t* version) {
    furi_check(start);
    furi_check(version);
    uint8_t* picobin_ptr = find_picobin_block(start, 4096 - PICOBIN_MAX_BLOCK_SIZE);
    if(!picobin_ptr) return false;

    size_t version_size = 2;
    uint8_t* version_item_ptr = find_picobin_item(picobin_ptr, PICOBIN_MAX_BLOCK_SIZE, PICOBIN_BLOCK_ITEM_1BS_VERSION, &version_size);

    if(version_item_ptr && version_size == 2) {
        version[1] = *(uint16_t*)&version_item_ptr[4];
        version[0] = *(uint16_t*)&version_item_ptr[6];
        return true;
    }

    return false;
}

bool furi_hal_flash_set_image_version(uint8_t* start, const uint16_t* version) {
    furi_check(start);
    furi_check(version);
    uint8_t* picobin_ptr = find_picobin_block(start, 4096 - PICOBIN_MAX_BLOCK_SIZE);
    if(!picobin_ptr) return false;

    size_t version_size = 2;
    uint8_t* version_item_ptr = find_picobin_item(picobin_ptr, PICOBIN_MAX_BLOCK_SIZE, PICOBIN_BLOCK_ITEM_1BS_VERSION, &version_size);

    if(version_item_ptr && version_size == 2) {
        *(uint16_t*)&version_item_ptr[4] = version[1];
        *(uint16_t*)&version_item_ptr[6] = version[0];
        return true;
    }

    return false;
}

void furi_hal_flash_erase_sector(size_t address) {
    FURI_CRITICAL_ENTER();
    flash_range_erase(address, FLASH_SECTOR_SIZE);
    FURI_CRITICAL_EXIT();
}

void furi_hal_flash_write(size_t address, const void* data, size_t size) {
    furi_check(data);
    furi_check(size % FLASH_PAGE_SIZE == 0);
    FURI_CRITICAL_ENTER();
    flash_range_program(address, data, size);
    FURI_CRITICAL_EXIT();
}

void furi_hal_flash_read(size_t address, void* data, size_t size) {
    furi_check(data);
    const uint8_t* flash_ptr = (const uint8_t*)(XIP_NOCACHE_NOALLOC_NOTRANSLATE_BASE + address);
    memcpy(data, flash_ptr, size);
}

const uint8_t* furi_hal_flash_get_read_ptr(size_t address) {
    const uint8_t* flash_ptr = (const uint8_t*)(XIP_NOCACHE_NOALLOC_NOTRANSLATE_BASE + address);
    return flash_ptr;
}

void furi_hal_flash_flush_cache(void) {
    FURI_CRITICAL_ENTER();
    rom_flash_flush_cache();
    FURI_CRITICAL_EXIT();
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
