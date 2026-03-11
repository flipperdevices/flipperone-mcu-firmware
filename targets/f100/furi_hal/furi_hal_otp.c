#include <furi.h>
#include "furi_hal_otp.h"
#include "pico/bootrom.h"
#include "hardware/structs/otp.h"
#include "hardware/regs/otp_data.h"
#include <string.h>

#define TAG "FuriHalOtp"

// Row in OTP where the USB white label table starts (must be page-aligned recommended)
#define WL_BASE_ROW 0x0400

// STRDEF encoding: high byte = row offset from WL_BASE_ROW, low byte = char count (ASCII)
#define WL_STRDEF(row_offset, char_count) (((row_offset) << 8) | (char_count))

// Helper macros to calculate string lengths and row counts at compile time
#define STR_LEN(str) (sizeof(str) - 1)

// Number of ECC rows a string occupies: 2 ASCII chars per row, ceil(len/2) = sizeof(str)/2
#define STR_ROWS(str) (sizeof(str) / 2)

// clang-format off

// USB device descriptor values
#define OTP_USB_VID          0x37C1
#define OTP_USB_PID          0xF102
#define OTP_USB_BCD          0x0100
#define OTP_USB_LANG_ID      0x0409
#define OTP_USB_MAX_POWER    0xFA
#define OTP_USB_ATTRIBUTES   0x80
#define OTP_USB_MANUFACTURER "Flipper FZCO"               // max 30 chars
#define OTP_USB_PRODUCT      "Flipper One MCU Bootloader" // max 30 chars

// USB MSD / SCSI strings
#define OTP_USB_VOLUME_LABEL "FlipOneMCU"  // max 11 chars
#define OTP_SCSI_VENDOR      "Flipper"     // max 8 chars
#define OTP_SCSI_PRODUCT     "Flipper One" // max 16 chars
#define OTP_SCSI_VERSION     "1.00"        // max 4 chars

// Bootloader info page strings
#define OTP_REDIRECT_URL     "https://r.flipper.net/flipper_one_mcu_update" // max 127 chars
#define OTP_REDIRECT_NAME    "How to Update Firmware" // max 127 chars
#define OTP_MODEL            "Flipper One"            // max 127 chars
#define OTP_BOARD_ID         "F0B0C1"                 // max 127 chars

// String row offsets from WL_BASE_ROW (strings are packed right after the 16-row table)
#define WL_OFF_MANUFACTURER  0x10
#define WL_OFF_PRODUCT       (WL_OFF_MANUFACTURER + STR_ROWS(OTP_USB_MANUFACTURER))
#define WL_OFF_VOLUME_LABEL  (WL_OFF_PRODUCT      + STR_ROWS(OTP_USB_PRODUCT))
#define WL_OFF_SCSI_VENDOR   (WL_OFF_VOLUME_LABEL + STR_ROWS(OTP_USB_VOLUME_LABEL))
#define WL_OFF_SCSI_PRODUCT  (WL_OFF_SCSI_VENDOR  + STR_ROWS(OTP_SCSI_VENDOR))
#define WL_OFF_SCSI_VERSION  (WL_OFF_SCSI_PRODUCT + STR_ROWS(OTP_SCSI_PRODUCT))
#define WL_OFF_REDIRECT_URL  (WL_OFF_SCSI_VERSION + STR_ROWS(OTP_SCSI_VERSION))
#define WL_OFF_REDIRECT_NAME (WL_OFF_REDIRECT_URL  + STR_ROWS(OTP_REDIRECT_URL))
#define WL_OFF_MODEL         (WL_OFF_REDIRECT_NAME + STR_ROWS(OTP_REDIRECT_NAME))
#define WL_OFF_BOARD_ID      (WL_OFF_MODEL         + STR_ROWS(OTP_MODEL))

static_assert(STR_LEN(OTP_USB_MANUFACTURER) <= 30,  "OTP_USB_MANUFACTURER too long");
static_assert(STR_LEN(OTP_USB_PRODUCT)      <= 30,  "OTP_USB_PRODUCT too long");
static_assert(STR_LEN(OTP_USB_VOLUME_LABEL) <= 11,  "OTP_USB_VOLUME_LABEL too long");
static_assert(STR_LEN(OTP_SCSI_VENDOR)      <= 8,   "OTP_SCSI_VENDOR too long");
static_assert(STR_LEN(OTP_SCSI_PRODUCT)     <= 16,  "OTP_SCSI_PRODUCT too long");
static_assert(STR_LEN(OTP_SCSI_VERSION)     <= 4,   "OTP_SCSI_VERSION too long");
static_assert(STR_LEN(OTP_REDIRECT_URL)     <= 127, "OTP_REDIRECT_URL too long");
static_assert(STR_LEN(OTP_REDIRECT_NAME)    <= 127, "OTP_REDIRECT_NAME too long");
static_assert(STR_LEN(OTP_MODEL)            <= 127, "OTP_MODEL too long");
static_assert(STR_LEN(OTP_BOARD_ID)         <= 127, "OTP_BOARD_ID too long");

// clang-format on

static bool furi_hal_otp_usb_white_label_addr_valid(void);

static bool furi_hal_otp_write_usb_white_label(void);

void furi_hal_otp_init(void) {
    FURI_LOG_I(TAG, "OTP init");

    if(!furi_hal_otp_usb_white_label_addr_valid()) {
        FURI_LOG_E(TAG, "USB white label address is not valid, writing white label to OTP");
        if(!furi_hal_otp_write_usb_white_label()) {
            FURI_LOG_E(TAG, "Failed to write USB white label to OTP");
        } else {
            FURI_LOG_I(TAG, "USB white label successfully written to OTP");
        }
    } else {
        FURI_LOG_I(TAG, "USB white label address is valid");
    }
}

static bool furi_hal_otp_write_ecc(uint16_t row, uint16_t value) {
    otp_cmd_t cmd = {.flags = row | OTP_CMD_ECC_BITS | OTP_CMD_WRITE_BITS};
    int rc = rom_func_otp_access((uint8_t*)&value, sizeof(value), cmd);
    if(rc != BOOTROM_OK) {
        FURI_LOG_E(TAG, "OTP ECC write row 0x%04x failed: %d", row, rc);
        return false;
    }
    return true;
}

static bool furi_hal_otp_write_raw(uint16_t row, uint32_t value) {
    otp_cmd_t cmd = {.flags = row | OTP_CMD_WRITE_BITS};
    int rc = rom_func_otp_access((uint8_t*)&value, sizeof(value), cmd);
    if(rc != BOOTROM_OK) {
        FURI_LOG_E(TAG, "OTP raw write row 0x%04x failed: %d", row, rc);
        return false;
    }
    return true;
}

// Write ASCII string as ECC rows: 2 chars packed per 16-bit ECC row
static bool furi_hal_otp_usb_white_label_write_string(uint16_t base_row, uint8_t row_offset, const char* str) {
    size_t len = strlen(str);
    uint8_t num_rows = (len + 1) / 2;
    uint16_t buf[num_rows];
    memset(buf, 0, sizeof(buf));
    for(uint8_t i = 0; i < len; i++) {
        if(i & 1)
            buf[i / 2] |= (uint8_t)str[i] << 8;
        else
            buf[i / 2] = (uint8_t)str[i];
    }
    otp_cmd_t cmd = {.flags = (base_row + row_offset) | OTP_CMD_ECC_BITS | OTP_CMD_WRITE_BITS};
    int rc = rom_func_otp_access((uint8_t*)buf, sizeof(buf), cmd);
    if(rc != BOOTROM_OK) {
        FURI_LOG_E(TAG, "OTP string write row 0x%04x failed: %d", base_row + row_offset, rc);
        return false;
    }
    return true;
}

bool furi_hal_otp_usb_white_label_addr_valid(void) {
    // USB_BOOT_FLAGS is a 3x redundant 24-bit row (no ECC)
    // Raw mode: 4 bytes per row, low 24 bits = OTP value
    uint32_t buf = 0;
    otp_cmd_t cmd = {.flags = OTP_DATA_USB_BOOT_FLAGS_ROW};
    int rc = rom_func_otp_access((uint8_t*)&buf, sizeof(buf), cmd);
    if(rc != BOOTROM_OK) {
        FURI_LOG_E(TAG, "OTP read failed: %d", rc);
        return false;
    }
    return (buf & OTP_DATA_USB_BOOT_FLAGS_WHITE_LABEL_ADDR_VALID_BITS) != 0;
}

bool furi_hal_otp_write_usb_white_label(void) {
    // USB white label table: 16 ECC rows at WL_BASE_ROW (indices per Table 579 in RP2350 datasheet)
    // String data follows the table, 2 ASCII chars per ECC row, packed little-endian

    // clang-format off
    const uint16_t wl_table[16] = {
        [0]  = OTP_USB_VID,                                                    // VID
        [1]  = OTP_USB_PID,                                                    // PID
        [2]  = OTP_USB_BCD,                                                    // bcdDevice 1.00
        [3]  = OTP_USB_LANG_ID,                                                // lang_id (English US)
        [4]  = WL_STRDEF(WL_OFF_MANUFACTURER, STR_LEN(OTP_USB_MANUFACTURER)),  // manufacturer
        [5]  = WL_STRDEF(WL_OFF_PRODUCT,      STR_LEN(OTP_USB_PRODUCT)),       // product
        [6]  = 0x0000,                                                         // serial number string (not set)
        [7]  = (OTP_USB_MAX_POWER << 8) | OTP_USB_ATTRIBUTES,                  // bMaxPower, bmAttributes
        [8]  = WL_STRDEF(WL_OFF_VOLUME_LABEL,  STR_LEN(OTP_USB_VOLUME_LABEL)), // volume label
        [9]  = WL_STRDEF(WL_OFF_SCSI_VENDOR,   STR_LEN(OTP_SCSI_VENDOR)),      // SCSI vendor
        [10] = WL_STRDEF(WL_OFF_SCSI_PRODUCT,  STR_LEN(OTP_SCSI_PRODUCT)),     // SCSI product
        [11] = WL_STRDEF(WL_OFF_SCSI_VERSION,  STR_LEN(OTP_SCSI_VERSION)),     // SCSI version
        [12] = WL_STRDEF(WL_OFF_REDIRECT_URL,  STR_LEN(OTP_REDIRECT_URL)),     // redirect URL
        [13] = WL_STRDEF(WL_OFF_REDIRECT_NAME, STR_LEN(OTP_REDIRECT_NAME)),    // redirect name
        [14] = WL_STRDEF(WL_OFF_MODEL,         STR_LEN(OTP_MODEL)),            // model
        [15] = WL_STRDEF(WL_OFF_BOARD_ID,      STR_LEN(OTP_BOARD_ID)),         // board_id
    };
    // clang-format on

    // 1. Write white label table (16 consecutive ECC rows)
    otp_cmd_t cmd = {.flags = WL_BASE_ROW | OTP_CMD_ECC_BITS | OTP_CMD_WRITE_BITS};
    int rc = rom_func_otp_access((uint8_t*)wl_table, sizeof(wl_table), cmd);
    if(rc != BOOTROM_OK) {
        FURI_LOG_E(TAG, "OTP white label table write failed: %d", rc);
        return false;
    }

    // 2. Write string data
    if(!furi_hal_otp_usb_white_label_write_string(WL_BASE_ROW, WL_OFF_MANUFACTURER, OTP_USB_MANUFACTURER)) return false;
    if(!furi_hal_otp_usb_white_label_write_string(WL_BASE_ROW, WL_OFF_PRODUCT, OTP_USB_PRODUCT)) return false;
    if(!furi_hal_otp_usb_white_label_write_string(WL_BASE_ROW, WL_OFF_VOLUME_LABEL, OTP_USB_VOLUME_LABEL)) return false;
    if(!furi_hal_otp_usb_white_label_write_string(WL_BASE_ROW, WL_OFF_SCSI_VENDOR, OTP_SCSI_VENDOR)) return false;
    if(!furi_hal_otp_usb_white_label_write_string(WL_BASE_ROW, WL_OFF_SCSI_PRODUCT, OTP_SCSI_PRODUCT)) return false;
    if(!furi_hal_otp_usb_white_label_write_string(WL_BASE_ROW, WL_OFF_SCSI_VERSION, OTP_SCSI_VERSION)) return false;
    if(!furi_hal_otp_usb_white_label_write_string(WL_BASE_ROW, WL_OFF_REDIRECT_URL, OTP_REDIRECT_URL)) return false;
    if(!furi_hal_otp_usb_white_label_write_string(WL_BASE_ROW, WL_OFF_REDIRECT_NAME, OTP_REDIRECT_NAME)) return false;
    if(!furi_hal_otp_usb_white_label_write_string(WL_BASE_ROW, WL_OFF_MODEL, OTP_MODEL)) return false;
    if(!furi_hal_otp_usb_white_label_write_string(WL_BASE_ROW, WL_OFF_BOARD_ID, OTP_BOARD_ID)) return false;

    // 3. Write USB_WHITE_LABEL_ADDR (ECC) — points to WL_BASE_ROW
    if(!furi_hal_otp_write_ecc(OTP_DATA_USB_WHITE_LABEL_ADDR_ROW, WL_BASE_ROW)) return false;

    // clang-format off
    // 4. Write USB_BOOT_FLAGS (raw, 3× redundant) — mark all written entries valid
    const uint32_t usb_boot_flags =
        OTP_DATA_USB_BOOT_FLAGS_WHITE_LABEL_ADDR_VALID_BITS |
        OTP_DATA_USB_BOOT_FLAGS_WL_USB_DEVICE_VID_VALUE_VALID_BITS |
        OTP_DATA_USB_BOOT_FLAGS_WL_USB_DEVICE_PID_VALUE_VALID_BITS |
        OTP_DATA_USB_BOOT_FLAGS_WL_USB_DEVICE_SERIAL_NUMBER_VALUE_VALID_BITS | // entry 2: bcdDevice
        OTP_DATA_USB_BOOT_FLAGS_WL_USB_DEVICE_LANG_ID_VALUE_VALID_BITS |
        OTP_DATA_USB_BOOT_FLAGS_WL_USB_DEVICE_MANUFACTURER_STRDEF_VALID_BITS |
        OTP_DATA_USB_BOOT_FLAGS_WL_USB_DEVICE_PRODUCT_STRDEF_VALID_BITS |
        // bit 6 (SERIAL_NUMBER_STRDEF) intentionally not set — no serial string
        OTP_DATA_USB_BOOT_FLAGS_WL_USB_CONFIG_ATTRIBUTES_MAX_POWER_VALUES_VALID_BITS |
        OTP_DATA_USB_BOOT_FLAGS_WL_VOLUME_LABEL_STRDEF_VALID_BITS |
        OTP_DATA_USB_BOOT_FLAGS_WL_SCSI_INQUIRY_VENDOR_STRDEF_VALID_BITS |
        OTP_DATA_USB_BOOT_FLAGS_WL_SCSI_INQUIRY_PRODUCT_STRDEF_VALID_BITS |
        OTP_DATA_USB_BOOT_FLAGS_WL_SCSI_INQUIRY_VERSION_STRDEF_VALID_BITS |
        OTP_DATA_USB_BOOT_FLAGS_WL_INDEX_HTM_REDIRECT_URL_STRDEF_VALID_BITS |
        OTP_DATA_USB_BOOT_FLAGS_WL_INDEX_HTM_REDIRECT_NAME_STRDEF_VALID_BITS |
        OTP_DATA_USB_BOOT_FLAGS_WL_INFO_UF2_TXT_MODEL_STRDEF_VALID_BITS |
        OTP_DATA_USB_BOOT_FLAGS_WL_INFO_UF2_TXT_BOARD_ID_STRDEF_VALID_BITS;
    // clang-format on

    if(!furi_hal_otp_write_raw(OTP_DATA_USB_BOOT_FLAGS_ROW, usb_boot_flags)) return false;
    if(!furi_hal_otp_write_raw(OTP_DATA_USB_BOOT_FLAGS_R1_ROW, usb_boot_flags)) return false;
    if(!furi_hal_otp_write_raw(OTP_DATA_USB_BOOT_FLAGS_R2_ROW, usb_boot_flags)) return false;

    return true;
}
