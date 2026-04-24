#include "furi_hal_nvm.h"
#include <kvstore.h>
#include <blockdevice/flash.h>
#include <kvstore_logkvs.h>
#include <pico/btstack_flash_bank.h>

#define TAG "FuriHalNvm"

#define FURI_HAL_NVM_MAX_STR_SIZE 256

//#include "kvstore.h"

#define FURI_HAL_NVM_BANK_DEFAULT_SIZE (128 * 1024)
#define FURI_HAL_NVM_BANK_OFFSET       (PICO_FLASH_BANK_STORAGE_OFFSET - FURI_HAL_NVM_BANK_DEFAULT_SIZE)

bool kvs_init(void) {
    FURI_LOG_I(
        TAG,
        "Create a block device that uses 0x%08x->0x%08x(%uKB) areas of flash memory",
        XIP_BASE + FURI_HAL_NVM_BANK_OFFSET,
        XIP_BASE + FURI_HAL_NVM_BANK_OFFSET + FURI_HAL_NVM_BANK_DEFAULT_SIZE,
        FURI_HAL_NVM_BANK_DEFAULT_SIZE / 1024);
    blockdevice_t* bd = blockdevice_flash_create(FURI_HAL_NVM_BANK_OFFSET, FURI_HAL_NVM_BANK_DEFAULT_SIZE);

    FURI_LOG_I(TAG, "Create a Log structured Key-Value Store that uses a block device");
    kvs_t* kvs = kvs_logkvs_create(bd);

    if(!kvs) {
        FURI_LOG_E(TAG, "Failed to create Key-Value Store");
        return false;
    }

    FURI_LOG_I(TAG, "Assign to global Key-Value Store");
    kvs_assign(kvs);

    return true;
}

static void furi_hal_nvm_wipe(void) {
    // Erase the entire area used for NVM
    flash_range_erase(FURI_HAL_NVM_BANK_OFFSET, FURI_HAL_NVM_BANK_DEFAULT_SIZE);
}

FuriHalNvmBootMode furi_hal_nvm_get_boot_mode(void) {
    return FuriHalRtcBootModeDummy;
}

void furi_hal_nvm_set_fault_data(uint32_t value) {
    UNUSED(value);
    /* This function does nothing */
}

void furi_hal_nvm_init(void) {
    // Initialize Key-Value Store
    // Defalut setting uses 128KB of flash memory for KV store, at the end of flash memory
    if(!kvs_init()) {
        // ToDo: add notice that NWM is scribbled
        furi_crash("Failed to initialize Key-Value Store");
        furi_hal_nvm_wipe();
    }
}

void furi_hal_nvm_deinit(void) {
    // Deinitialize Key-Value Store
    kvs_t* kvs = kvs_global_instance();
    if(kvs) {
        kvs->deinit(kvs);
    }
}

static FuriHalNvmStorage furi_hal_nvm_check_error(const char* key, int rc) {
    furi_check(rc != -1); //non-initialized KVS
    if(rc == KVSTORE_SUCCESS) {
        return FuriHalNvmStorageOK;
    } else if(rc == KVSTORE_ERROR_ITEM_NOT_FOUND) {
        FURI_LOG_E(TAG, "Key = \"%s\", %s", key, kvs_strerror(rc));
        return FuriHalNvmStorageItemNotFound;
    } else {
        FURI_LOG_E(TAG, "Key = \"%s\", %s", key, kvs_strerror(rc));
        return FuriHalNvmStorageError;
    }
}

FuriHalNvmStorage furi_hal_nvm_delete(const char* key) {
    furi_check(key);
    int rc = kvs_delete(key);
    return furi_hal_nvm_check_error(key, rc);
}

FuriHalNvmStorage furi_hal_nvm_set_str(const char* key, FuriString* value) {
    furi_check(key);
    furi_check(value);
    int rc = kvs_set(key, furi_string_get_cstr(value), furi_string_size(value) + 1);
    return furi_hal_nvm_check_error(key, rc);
}
FuriHalNvmStorage furi_hal_nvm_get_str(const char* key, FuriString* value) {
    furi_check(key);
    furi_check(value);
    char* buffer = malloc(FURI_HAL_NVM_MAX_STR_SIZE);
    int rc = kvs_get_str(key, buffer, FURI_HAL_NVM_MAX_STR_SIZE);
    if(rc == KVSTORE_SUCCESS) {
        furi_string_set_str(value, buffer);
    }
    free(buffer);
    return furi_hal_nvm_check_error(key, rc);
}

FuriHalNvmStorage furi_hal_nvm_set_uint32(const char* key, uint32_t value) {
    furi_check(key);
    int rc = kvs_set(key, &value, sizeof(value));
    return furi_hal_nvm_check_error(key, rc);
}

FuriHalNvmStorage furi_hal_nvm_get_uint32(const char* key, uint32_t* value) {
    furi_check(key);
    furi_check(value);
    int rc = kvs_get(key, value, sizeof(*value), NULL);
    return furi_hal_nvm_check_error(key, rc);
}

FuriHalNvmStorage furi_hal_nvm_set_int32(const char* key, int32_t value) {
    furi_check(key);
    int rc = kvs_set(key, &value, sizeof(value));
    return furi_hal_nvm_check_error(key, rc);
}

FuriHalNvmStorage furi_hal_nvm_get_int32(const char* key, int32_t* value) {
    furi_check(key);
    furi_check(value);
    int rc = kvs_get(key, value, sizeof(*value), NULL);
    return furi_hal_nvm_check_error(key, rc);
}

FuriHalNvmStorage furi_hal_nvm_set_bool(const char* key, bool value) {
    furi_check(key);
    int rc = kvs_set(key, &value, sizeof(value));
    return furi_hal_nvm_check_error(key, rc);
}

FuriHalNvmStorage furi_hal_nvm_get_bool(const char* key, bool* value) {
    furi_check(key);
    furi_check(value);
    int rc = kvs_get(key, value, sizeof(*value), NULL);
    return furi_hal_nvm_check_error(key, rc);
}

FuriHalNvmStorage furi_hal_nvm_set_struct(const char* key, const void* ptr, size_t size) {
    furi_check(key);
    furi_check(ptr);
    int rc = kvs_set(key, ptr, size);
    return furi_hal_nvm_check_error(key, rc);
}

FuriHalNvmStorage furi_hal_nvm_get_struct(const char* key, void* ptr, size_t size) {
    furi_check(key);
    furi_check(ptr);
    int rc = kvs_get(key, ptr, size, NULL);
    return furi_hal_nvm_check_error(key, rc);
}
