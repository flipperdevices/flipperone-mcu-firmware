#pragma once

#include <furi.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FuriHalNvmStorageOK,
    FuriHalNvmStorageItemNotFound,
    FuriHalNvmStorageError,
} FuriHalNvmStorage;

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

/** Initialize NVM storage
 *
 */
void furi_hal_nvm_init(void);

/** Deinitialize NVM storage
 *
 */
void furi_hal_nvm_deinit(void);

/** Delete a key from NVM storage
 *
 * @param      key   The key
 * @return     Storage status
 */
FuriHalNvmStorage furi_hal_nvm_delete(const char* key);

/** Set a string value in NVM storage
 *
 * @param      key    The key
 * @param      value  The value
 * @return     Storage status
 */
FuriHalNvmStorage furi_hal_nvm_set_str(const char* key, FuriString* value);

/** Get a string value from NVM storage
 *
 * @param      key    The key
 * @param      value  The value
 * @return     Storage status
 */
FuriHalNvmStorage furi_hal_nvm_get_str(const char* key, FuriString* value);

/** Set a uint32_t value in NVM storage
 *
 * @param      key    The key
 * @param      value  The value
 * @return     Storage status
 */
FuriHalNvmStorage furi_hal_nvm_set_uint32(const char* key, uint32_t value);

/** Get a uint32_t value from NVM storage
 *
 * @param      key    The key
 * @param      value  The value
 * @return     Storage status
 */
FuriHalNvmStorage furi_hal_nvm_get_uint32(const char* key, uint32_t* value);

/** Set an int32_t value in NVM storage
 *
 * @param      key    The key
 * @param      value  The value
 * @return     Storage status
 */
FuriHalNvmStorage furi_hal_nvm_set_int32(const char* key, int32_t value);

/** Get an int32_t value from NVM storage
 *
 * @param      key    The key
 * @param      value  The value
 * @return     Storage status
 */
FuriHalNvmStorage furi_hal_nvm_get_int32(const char* key, int32_t* value);

/** Set a bool value in NVM storage
 *
 * @param      key    The key
 * @param      value  The value
 * @return     Storage status
 */
FuriHalNvmStorage furi_hal_nvm_set_bool(const char* key, bool value);

/** Get a bool value from NVM storage
 *
 * @param      key    The key
 * @param      value  The value
 * @return     Storage status
 */
FuriHalNvmStorage furi_hal_nvm_get_bool(const char* key, bool* value);

#ifdef __cplusplus
}
#endif
