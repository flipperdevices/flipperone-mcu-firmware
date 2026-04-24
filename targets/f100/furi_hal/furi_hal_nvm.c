#include "furi_hal_nvm.h"
#include <kvstore.h>

#define TAG "FuriHalNvm"

#define FURI_HAL_NVM_MAX_STR_SIZE 256

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
    kvs_init();
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

int kvs_get_safe(const char* key, void* value, size_t buffer_size, size_t* value_size) {
    FURI_CRITICAL_ENTER();
    int rc = kvs_get(key, value, buffer_size, value_size);
    FURI_CRITICAL_EXIT();
    return rc;
}

int kvs_get_str_safe(const char* key, char* value, size_t buffer_size) {
    FURI_CRITICAL_ENTER();
    int rc = kvs_get_str(key, value, buffer_size);
    FURI_CRITICAL_EXIT();
    return rc;
}

int kvs_set_safe(const char* key, const void* value, size_t size) {
    FURI_CRITICAL_ENTER();
    int rc = kvs_set(key, value, size);
    FURI_CRITICAL_EXIT();
    return rc;
}

int kvs_delete_safe(const char* key) {
    FURI_CRITICAL_ENTER();
    int rc = kvs_delete(key);
    FURI_CRITICAL_EXIT();
    return rc;
}

FuriHalNvmStorage furi_hal_nvm_delete(const char* key) {
    furi_check(key);
    int rc = kvs_delete_safe(key);
    return furi_hal_nvm_check_error(key, rc);
}

FuriHalNvmStorage furi_hal_nvm_set_str(const char* key, FuriString* value) {
    furi_check(key);
    furi_check(value);
    int rc = kvs_set_safe(key, furi_string_get_cstr(value), furi_string_size(value) + 1);
    return furi_hal_nvm_check_error(key, rc);
}
FuriHalNvmStorage furi_hal_nvm_get_str(const char* key, FuriString* value) {
    furi_check(key);
    furi_check(value);
    char* buffer = malloc(FURI_HAL_NVM_MAX_STR_SIZE);
    int rc = kvs_get_str_safe(key, buffer, FURI_HAL_NVM_MAX_STR_SIZE);
    if(rc == KVSTORE_SUCCESS) {
        furi_string_set_str(value, buffer);
    }
    free(buffer);
    return furi_hal_nvm_check_error(key, rc);
}

FuriHalNvmStorage furi_hal_nvm_set_uint32(const char* key, uint32_t value) {
    furi_check(key);
    int rc = kvs_set_safe(key, &value, sizeof(value));
    return furi_hal_nvm_check_error(key, rc);
}

FuriHalNvmStorage furi_hal_nvm_get_uint32(const char* key, uint32_t* value) {
    furi_check(key);
    furi_check(value);
    int rc = kvs_get_safe(key, value, sizeof(*value), NULL);
    return furi_hal_nvm_check_error(key, rc);
}

FuriHalNvmStorage furi_hal_nvm_set_int32(const char* key, int32_t value) {
    furi_check(key);
    int rc = kvs_set_safe(key, &value, sizeof(value));
    return furi_hal_nvm_check_error(key, rc);
}

FuriHalNvmStorage furi_hal_nvm_get_int32(const char* key, int32_t* value) {
    furi_check(key);
    furi_check(value);
    int rc = kvs_get_safe(key, value, sizeof(*value), NULL);
    return furi_hal_nvm_check_error(key, rc);
}

FuriHalNvmStorage furi_hal_nvm_set_bool(const char* key, bool value) {
    furi_check(key);
    int rc = kvs_set_safe(key, &value, sizeof(value));
    return furi_hal_nvm_check_error(key, rc);
}

FuriHalNvmStorage furi_hal_nvm_get_bool(const char* key, bool* value) {
    furi_check(key);
    furi_check(value);
    int rc = kvs_get_safe(key, value, sizeof(*value), NULL);
    return furi_hal_nvm_check_error(key, rc);
}
