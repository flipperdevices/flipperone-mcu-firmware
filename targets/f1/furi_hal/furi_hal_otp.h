#pragma once

#include <stdbool.h>
#include <stdint.h>

/*
 * OTP map (RP2350: 4096 rows of 24 bits, 64 pages of 64 rows)
 *
 * Rows           Pages   Owner
 * 0x000..0x03F   0       Factory: chip id, random id, calibration
 * 0x040..0x07F   1       Boot config read by the bootrom. Ours in it:
 *                          BOOTSEL_LED_CFG (0x056) and BOOT_FLAGS0.ENABLE_BOOTSEL_LED,
 *                          USB_BOOT_FLAGS (0x059..0x05B), USB_WHITE_LABEL_ADDR (0x05C)
 * 0x080..0x0BF   2       Boot keys
 * 0x0C0..0x0FF   3       OTP_DEVICE (FURI_HAL_OTP_DEVICE_ROW), locked once written
 * 0x100..0x13F   4       OTP_MAC (FURI_HAL_OTP_MAC_ROW), locked once written
 * 0x400..0x47F   16..17  USB white label, locked once written
 * 0xF40..0xF7F   61      Hardware keys
 * 0xF80..0xFFF   62..63  Page lock rows
 * Everything else is free.
 *
 * OTP_DEVICE and OTP_MAC are ECC rows, 16 data bits each:
 *   +0      magic (FURI_HAL_OTP_DEVICE_MAGIC / FURI_HAL_OTP_MAC_MAGIC)
 *   +1      layout version (FURI_HAL_OTP_LAYOUT_VERSION)
 *   +2..25  payload, 48 bytes, little endian
 * The magic is written last, so a block cut short is never taken as written.
 *
 * OTP_DEVICE payload: name (31 chars + NUL), version, target, body, connect
 * (2 bytes each), date (8 bytes, seconds since the epoch).
 * OTP_MAC payload: 8 MAC addresses of 6 bytes, in FuriHalOtpMac order.
 *
 * A page is locked by its PAGEn_LOCK1 row: read only for Secure, Non-secure
 * and bootloader access. The hardware applies it from the next reset; until
 * then the same lock is set in SW_LOCKn.
 */

#define FURI_HAL_OTP_DEVICE_ROW     0x0C0
#define FURI_HAL_OTP_MAC_ROW        0x100
#define FURI_HAL_OTP_DEVICE_MAGIC   0xF1DE
#define FURI_HAL_OTP_MAC_MAGIC      0xF1AC
#define FURI_HAL_OTP_LAYOUT_VERSION 1

#define FURI_HAL_OTP_DEVICE_NAME_SIZE 32 /* 31 chars + NUL */
#define FURI_HAL_OTP_MAC_SIZE         6

void furi_hal_otp_init(void);

typedef enum {
    FuriHalOtpErrorNone,
    FuriHalOtpErrorInvalidArg, /* A value does not fit the block */
    FuriHalOtpErrorAlreadyWritten, /* The block is written: OTP is written once */
    FuriHalOtpErrorNotBlank, /* The block holds data but no magic: a write was cut short */
    FuriHalOtpErrorWriteFailed,
    FuriHalOtpErrorLockFailed, /* Written, but its page is not locked */
} FuriHalOtpError;

const char* furi_hal_otp_error_to_string(FuriHalOtpError error);

/* OTP_DEVICE */

typedef struct {
    char name[FURI_HAL_OTP_DEVICE_NAME_SIZE]; /* NUL terminated */
    uint16_t version;
    uint16_t target;
    uint16_t body;
    uint16_t connect;
    uint64_t timestamp; /* seconds since the epoch */
} FuriHalOtpDevice;

/** Whether OTP_DEVICE holds a block of a known layout (by its magic) */
bool furi_hal_otp_device_valid(void);

/** Read OTP_DEVICE.
 * @return false if the block is not valid; device is then zeroed
 */
bool furi_hal_otp_device_read(FuriHalOtpDevice* device);

/** Write OTP_DEVICE once and lock its page. Irreversible. */
FuriHalOtpError furi_hal_otp_device_write(const FuriHalOtpDevice* device);

/* OTP_MAC */

typedef enum {
    FuriHalOtpMacEth0,
    FuriHalOtpMacEth1,
    FuriHalOtpMacUsb,
    FuriHalOtpMacWifi24Sta,
    FuriHalOtpMacWifi5Sta,
    FuriHalOtpMacWifi24Ap,
    FuriHalOtpMacWifi5Ap,
    FuriHalOtpMacBt,
    FuriHalOtpMacCount,
} FuriHalOtpMac;

typedef struct {
    uint8_t mac[FuriHalOtpMacCount][FURI_HAL_OTP_MAC_SIZE];
} FuriHalOtpMacs;

/** Name of a MAC address slot, as the CLI shows it */
const char* furi_hal_otp_mac_name(FuriHalOtpMac mac);

/** Whether OTP_MAC holds a block of a known layout (by its magic) */
bool furi_hal_otp_mac_valid(void);

/** Read OTP_MAC.
 * @return false if the block is not valid; macs is then zeroed
 */
bool furi_hal_otp_mac_read(FuriHalOtpMacs* macs);

/** Write OTP_MAC once and lock its page. Irreversible. */
FuriHalOtpError furi_hal_otp_mac_write(const FuriHalOtpMacs* macs);

/* USB white label */

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
    FuriHalOtpUsbWhiteLabelErrorLockFailed,
} FuriHalOtpUsbWhiteLabelError;

bool furi_hal_otp_usb_white_label_valid(void);

/** Write the USB white label once, its board id "F<target>B<body>C<connect>",
 * and lock its pages. Irreversible. */
FuriHalOtpUsbWhiteLabelError furi_hal_otp_write_usb_white_label(uint32_t target, uint32_t body, uint32_t connect);

/** Lock the pages of a white label written before the lock existed. */
bool furi_hal_otp_usb_white_label_lock(void);

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
