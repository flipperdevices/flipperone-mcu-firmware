#include <furi.h>
#include <hardware/flash.h>
#include <hardware/regs/addressmap.h>
#include <pico/flash.h>
#include <pico/mutex.h>
#include "blockdevice/flash.h"
#include "pico/bootrom.h"
#include "boot/picobin.h"

#define TAG "FuriHalNvmStorage"

#define NVM_PARTITION_ID 2 // Third partition after A+B

#define FLASH_SAFE_EXECUTE_TIMEOUT 10 * 1000

#define FLASH_BLOCK_DEVICE_ERROR_TIMEOUT                -4001 /*!< operation timeout */
#define FLASH_BLOCK_DEVICE_ERROR_NOT_PERMITTED          -4002 /*!< safe execution is not possible */
#define FLASH_BLOCK_DEVICE_ERROR_INSUFFICIENT_RESOURCES -4003 /*!< method fails due to dynamic resource exhaustion */

typedef struct {
    uint32_t start;
    size_t length;
    mutex_t _mutex;
} blockdevice_flash_config_t;

typedef struct {
    bool is_erase;
    size_t addr;
    size_t size;
    void* buffer;
} _safe_flash_update_param_t;

static const char DEVICE_NAME[] = "flash";

static int _error_remap(int err) {
    switch(err) {
    case PICO_OK:
        return BD_ERROR_OK;
    case PICO_ERROR_TIMEOUT:
        return FLASH_BLOCK_DEVICE_ERROR_TIMEOUT;
    case PICO_ERROR_NOT_PERMITTED:
        return FLASH_BLOCK_DEVICE_ERROR_NOT_PERMITTED;
    case PICO_ERROR_INSUFFICIENT_RESOURCES:
        return FLASH_BLOCK_DEVICE_ERROR_INSUFFICIENT_RESOURCES;
    default:
        return err;
    }
}

static size_t flash_target_offset(blockdevice_t* device) {
    blockdevice_flash_config_t* config = device->config;
    return config->start;
}

static int init(blockdevice_t* device) {
    device->is_initialized = true;
    return BD_ERROR_OK;
}

static int deinit(blockdevice_t* device) {
    device->is_initialized = false;
    return 0;
}

static int sync(blockdevice_t* device) {
    (void)device;
    return 0;
}

static int read(blockdevice_t* device, const void* buffer, bd_size_t addr, bd_size_t size) {
    blockdevice_flash_config_t* config = device->config;

    mutex_enter_blocking(&config->_mutex);
    const uint8_t* flash_contents = (const uint8_t*)(XIP_NOCACHE_NOALLOC_NOTRANSLATE_BASE + flash_target_offset(device) + (size_t)addr);
    memcpy((uint8_t*)buffer, flash_contents, (size_t)size);
    mutex_exit(&config->_mutex);

    return BD_ERROR_OK;
}

static void _safe_flash_update(void* param) {
    const _safe_flash_update_param_t* args = param;
    if(args->is_erase) {
        flash_range_erase(args->addr, args->size);
    } else {
        flash_range_program(args->addr, (const uint8_t*)args->buffer, args->size);
    }
}

static int erase(blockdevice_t* device, bd_size_t addr, bd_size_t size) {
    _safe_flash_update_param_t param = {
        .is_erase = true,
        .addr = flash_target_offset(device) + addr,
        .size = (size_t)size,
    };
    int err = flash_safe_execute(_safe_flash_update, &param, FLASH_SAFE_EXECUTE_TIMEOUT);
    return _error_remap(err);
}

static int program(blockdevice_t* device, const void* buffer, bd_size_t addr, bd_size_t size) {
    _safe_flash_update_param_t param = {
        .is_erase = false,
        .addr = flash_target_offset(device) + addr,
        .buffer = (void*)buffer,
        .size = (size_t)size,
    };
    int err = flash_safe_execute(_safe_flash_update, &param, FLASH_SAFE_EXECUTE_TIMEOUT);
    return _error_remap(err);
}

static int trim(blockdevice_t* device, bd_size_t addr, bd_size_t size) {
    (void)device;
    (void)addr;
    (void)size;
    return BD_ERROR_OK;
}

static bd_size_t size(blockdevice_t* device) {
    blockdevice_flash_config_t* config = device->config;
    return (bd_size_t)config->length;
}

blockdevice_t* furi_hal_nvm_storage_init(void) {
    uint32_t nvm_start = 0;
    size_t nvm_length = 0;

    // See datasheet 5.4.8.16; 5.9.4.2 for more details
    uint32_t buffer[(3 * 4) + 1] = {0}; // maximum of 4 partitions, each with maximum of 4 words returned, plus 1 for header
    int ret = rom_get_partition_table_info(buffer, COUNT_OF(buffer), PT_INFO_PARTITION_LOCATION_AND_FLAGS | PT_INFO_PARTITION_ID);

    furi_check(buffer[0] == (PT_INFO_PARTITION_LOCATION_AND_FLAGS | PT_INFO_PARTITION_ID));

    uint32_t partition_count = (ret - 1) / 4;
    for(size_t i = 0; i < partition_count; i++) {
        size_t offset = 1 + i * 4;

        uint32_t id = buffer[offset + 2];
        if(id == NVM_PARTITION_ID) {
            uint32_t start = (buffer[offset] & PICOBIN_PARTITION_LOCATION_FIRST_SECTOR_BITS) >> PICOBIN_PARTITION_LOCATION_FIRST_SECTOR_LSB;
            uint32_t end = (buffer[offset] & PICOBIN_PARTITION_LOCATION_LAST_SECTOR_BITS) >> PICOBIN_PARTITION_LOCATION_LAST_SECTOR_LSB;
            nvm_length = (end - start + 1) * 4096;
            nvm_start = start * 4096;
            break;
        }
    }

    FURI_LOG_I(TAG, "NVM partition: 0x%08lx %uk", nvm_start, nvm_length / 1024);

    blockdevice_t* device = malloc(sizeof(blockdevice_t));
    blockdevice_flash_config_t* config = malloc(sizeof(blockdevice_flash_config_t));

    config->start = nvm_start;
    config->length = nvm_length;

    furi_check(config->start != 0 && config->length != 0);
    furi_check(config->start % FLASH_SECTOR_SIZE == 0);
    furi_check(config->length % FLASH_SECTOR_SIZE == 0);

    device->init = init;
    device->deinit = deinit;
    device->read = read;
    device->erase = erase;
    device->program = program;
    device->trim = trim;
    device->sync = sync;
    device->size = size;
    device->read_size = 1;
    device->erase_size = FLASH_SECTOR_SIZE; // 4096 byte
    device->program_size = FLASH_PAGE_SIZE; // 256 byte
    device->name = DEVICE_NAME;
    device->is_initialized = false;

    mutex_init(&config->_mutex);
    device->config = config;
    device->init(device);
    return device;
}

void furi_hal_nvm_storage_wipe(blockdevice_t* device) {
    furi_check(device);
    furi_check(device->config);
    device->erase(device, 0, device->size(device));
}

void furi_hal_nvm_storage_deinit(blockdevice_t* device) {
    furi_check(device);
    furi_check(device->config);
    free(device->config);
    free(device);
}
