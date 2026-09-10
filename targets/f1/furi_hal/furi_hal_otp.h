#pragma once

#include <stdbool.h>

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