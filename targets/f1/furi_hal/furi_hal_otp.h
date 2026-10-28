#pragma once

#include <stdbool.h>
#include <stddef.h>

void furi_hal_otp_init(void);

typedef enum {
    FuriHalOtpUsbWhiteLabelErrorNone,
    FuriHalOtpUsbWhiteLabelErrorBoardIdTooLong,
    FuriHalOtpUsbWhiteLabelErrorWriteTableFailed,
    FuriHalOtpUsbWhiteLabelErrorWriteManufacturerFailed,
    FuriHalOtpUsbWhiteLabelErrorWriteProductFailed,
    FuriHalOtpUsbWhiteLabelErrorWriteVolumeLabelFailed,
    FuriHalOtpUsbWhiteLabelErrorWriteScsiVendorFailed,
    FuriHalOtpUsbWhiteLabelErrorWriteScsiProductFailed,
    FuriHalOtpUsbWhiteLabelErrorWriteScsiVersionFailed,
    FuriHalOtpUsbWhiteLabelErrorWriteRedirectUrlFailed,
    FuriHalOtpUsbWhiteLabelErrorWriteRedirectNameFailed,
    FuriHalOtpUsbWhiteLabelErrorWriteModelFailed,
    FuriHalOtpUsbWhiteLabelErrorWriteBoardIdFailed,
    FuriHalOtpUsbWhiteLabelErrorWriteAddrFailed,
    FuriHalOtpUsbWhiteLabelErrorWriteBootFlagsFailed,
    FuriHalOtpUsbWhiteLabelErrorWriteBootFlagsR1Failed,
    FuriHalOtpUsbWhiteLabelErrorWriteBootFlagsR2Failed,
} FuriHalOtpUsbWhiteLabelError;

bool furi_hal_otp_usb_white_label_valid(void);

FuriHalOtpUsbWhiteLabelError furi_hal_otp_write_usb_white_label(uint32_t firmware_id, uint32_t body_id, uint32_t connectivity_id);

const char* furi_hal_otp_usb_white_label_error_to_string(FuriHalOtpUsbWhiteLabelError error);

/** Check if BOOTSEL LED is enabled in OTP (BOOT_FLAGS0.ENABLE_BOOTSEL_LED)
 *
 * @return true if enabled
 */
bool furi_hal_otp_bootsel_led_valid(void);

/** Enable BOOTSEL LED: writes BOOTSEL_LED_CFG (GPIO 40, active high)
 * and sets BOOT_FLAGS0.ENABLE_BOOTSEL_LED. OTP write is irreversible.
 *
 * @return true on success
 */
bool furi_hal_otp_write_bootsel_led(void);

typedef enum {
    FuriHalOtpDeviceNameErrorNone,
    FuriHalOtpDeviceNameErrorInvalidName,
    FuriHalOtpDeviceNameErrorReadFailed,
    FuriHalOtpDeviceNameErrorAlreadyProgrammed,
    FuriHalOtpDeviceNameErrorWriteNameFailed,
    FuriHalOtpDeviceNameErrorWriteStrdefFailed,
    FuriHalOtpDeviceNameErrorVerifyFailed,
} FuriHalOtpDeviceNameError;

/** Check if a string can be used as a device name:
 * 1 to FURI_HAL_VERSION_NAME_LENGTH ASCII letters and digits
 *
 * @param name C-string
 * @return true if valid
 */
bool furi_hal_otp_device_name_is_valid(const char* name);

/** Read the device name from OTP
 *
 * @param name buffer for the C-string
 * @param name_size buffer size, at least FURI_HAL_VERSION_ARRAY_NAME_LENGTH
 * @return true if a valid name is programmed, false otherwise (name is set to "")
 */
bool furi_hal_otp_read_device_name(char* name, size_t name_size);

/** Program the device name into OTP. OTP write is irreversible.
 *
 * @param name C-string, see furi_hal_otp_device_name_is_valid()
 * @return FuriHalOtpDeviceNameErrorNone on success
 */
FuriHalOtpDeviceNameError furi_hal_otp_write_device_name(const char* name);

const char* furi_hal_otp_device_name_error_to_string(FuriHalOtpDeviceNameError error);
