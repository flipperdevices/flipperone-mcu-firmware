/**
 * @file furi_hal_version.h
 * Version HAL API
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <version/version.h>
#include <furi.h>
#include <furi_hal_otp.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FURI_HAL_VERSION_NAME_LENGTH        (FURI_HAL_OTP_STR_SIZE - 1)
#define FURI_HAL_VERSION_ARRAY_NAME_LENGTH  (FURI_HAL_VERSION_NAME_LENGTH + 1)
/** BLE symbol + "Flipper " + name */
#define FURI_HAL_VERSION_DEVICE_NAME_LENGTH (1 + 8 + FURI_HAL_VERSION_ARRAY_NAME_LENGTH)

/** OTP Versions enum */
typedef enum {
    FuriHalVersionOtpVersion0 = 0x00,
    FuriHalVersionOtpVersion1 = 0x01,
    FuriHalVersionOtpVersion2 = 0x02,
    FuriHalVersionOtpVersionEmpty = 0xFFFFFFFE,
    FuriHalVersionOtpVersionUnknown = 0xFFFFFFFF,
} FuriHalVersionOtpVersion;

/** Init flipper version
 */
void furi_hal_version_init(void);

/** Check target firmware version
 *
 * @return     true if target and real matches
 */
bool furi_hal_version_do_i_belong_here(void);

/** Get model name
 *
 * @return     model name C-string
 */
const char* furi_hal_version_get_model_name(void);

/** Get model name
 *
 * @return     model code C-string
 */
const char* furi_hal_version_get_model_code(void);

/** Get FCC ID
 *
 * @return     FCC id as C-string
 */
const char* furi_hal_version_get_fcc_id(void);

/** Get IC id
 *
 * @return     IC id as C-string
 */
const char* furi_hal_version_get_ic_id(void);

/** Get MIC id
 *
 * @return     MIC id as C-string
 */
const char* furi_hal_version_get_mic_id(void);

/** Get SRRC id
 *
 * @return     SRRC id as C-string
 */
const char* furi_hal_version_get_srrc_id(void);

/** Get NCC id
 *
 * @return     NCC id as C-string
 */
const char* furi_hal_version_get_ncc_id(void);

/** Get OTP version
 *
 * @return     OTP Version
 */
FuriHalVersionOtpVersion furi_hal_version_get_otp_version(void);

/** Get hardware version
 *
 * @return     Hardware Version
 */
uint16_t furi_hal_version_get_hw_version(void);

/** Get hardware target
 *
 * @return     Hardware Target
 */
uint16_t furi_hal_version_get_hw_target(void);

/** Get hardware body
 *
 * @return     Hardware Body
 */
uint16_t furi_hal_version_get_hw_body(void);

/** Get hardware body color, as OTP block 2 holds it
 *
 * @return     Hardware Color, 0 if unknown
 */
uint16_t furi_hal_version_get_hw_color(void);

/** Get hardware connect
 *
 * @return     Hardware Interconnect
 */
uint16_t furi_hal_version_get_hw_connect(void);

/** Get hardware region name, two letters as OTP block 2 holds them
 *
 * @return     Hardware Region name, "Unknown" if unknown
 */
const char* furi_hal_version_get_hw_region_name(void);

/** Get hardware display id, as OTP block 2 holds it
 *
 * @return     Display id, 0 if unknown
 */
uint16_t furi_hal_version_get_hw_display(void);

/** Whether an OTP block is written whole, of a known version, with a right CRC */
bool furi_hal_version_is_otp_valid(FuriHalOtpBlock block);

/** Whether the bootrom takes the USB white label OTP block 1 writes as written */
bool furi_hal_version_is_usb_white_label_valid(void);

/** Whether all OTP blocks are written and their signatures verify */
bool furi_hal_version_is_otp_signed(void);

/** When an OTP block was made, seconds since the epoch, 0 if not written */
uint64_t furi_hal_version_get_otp_timestamp(FuriHalOtpBlock block);

/** Chip id OTP block 1 holds, NULL if not written */
const uint8_t* furi_hal_version_get_otp_chip_id(void);

/** A MAC address OTP block 2 holds, NULL if not written */
const uint8_t* furi_hal_version_get_mac(FuriHalOtpMac mac);

/** CPU id OTP block 2 holds, NULL if not written */
const uint8_t* furi_hal_version_get_cpu_id(void);

/** RAM size OTP block 2 holds, in GB, 0 if not written */
uint32_t furi_hal_version_get_ram_gb(void);

/** UFS size OTP block 2 holds, in GB, 0 if not written */
uint32_t furi_hal_version_get_ufs_gb(void);

/** Key id OTP block 3 holds, 0 if not written */
uint16_t furi_hal_version_get_sign_key_id(void);

/** Curve OTP block 3 holds (FuriHalOtpCurve), 0 if not written */
uint16_t furi_hal_version_get_sign_curve(void);

/** Public key OTP block 3 holds, X || Y, NULL if not written */
const uint8_t* furi_hal_version_get_sign_public_key(void);

/** Signature OTP block 3 holds of block 1 or 2, NULL if not written */
const uint8_t* furi_hal_version_get_otp_signature(FuriHalOtpBlock block);

/** Get hardware timestamp
 *
 * @return     Hardware Manufacture timestamp
 */
uint64_t furi_hal_version_get_hw_timestamp(void);

/** Get hardware version code "<version>.F<target>.B<body>.C<connect>", as
 * OTP block 1 holds it, or "Unknown"
 *
 * @return     Hardware version code C-string
 */
const char* furi_hal_version_get_hw_version_code(void);

/** Get model name OTP block 2 holds, or "Unknown"
 *
 * @return     Model name C-string
 */
const char* furi_hal_version_get_model_ptr(void);

/** Get pointer to target name
 *
 * @return     Hardware Name C-string
 */
const char* furi_hal_version_get_name_ptr(void);

/** Get pointer to target device name
 *
 * @return     Hardware Device Name C-string
 */
const char* furi_hal_version_get_device_name_ptr(void);

/** Get pointer to target ble local device name
 *
 * @return     Ble Device Name C-string
 */
const char* furi_hal_version_get_ble_local_device_name_ptr(void);

/** Get BLE MAC address
 *
 * @return     pointer to BLE MAC address
 */
const uint8_t* furi_hal_version_get_ble_mac(void);

/** Get address of version structure of firmware.
 *
 * @return     Address of firmware version structure.
 */
const struct Version* furi_hal_version_get_firmware_version(void);

/** Get platform UID size in bytes
 *
 * @return     UID size in bytes
 */
size_t furi_hal_version_uid_size(void);

/** Get const pointer to UID
 *
 * @return     pointer to UID
 */
const uint8_t* furi_hal_version_uid(void);

/** Get device serial number (UID) as a string
  *
  * @param[in]  uid     a string to store the value
  */
void furi_hal_version_get_uid_str(FuriString* uid);

#ifdef __cplusplus
}
#endif
