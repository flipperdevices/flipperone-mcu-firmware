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
static bool furi_hal_otp_write_string(uint16_t base_row, uint8_t row_offset, const char* str) {
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
    //
    // String layout (offsets from WL_BASE_ROW):
    //   0x10 "Flipper FZCO"                              (12 chars, 6 rows), manufacturer string, max-length 30 UTF-16 or ASCII chars
    //   0x16 "Flipper One MCU Bootloader"                (26 chars, 13 rows), product string, max-length 30 UTF-16 or ASCII chars
    //   0x23 "FlipOneMCU"                                (10 chars, 5 rows), volume label must be a string with < 11 characters
    //   0x28 "Flipper"                                   ( 7 chars, 4 rows), SCSI vendor must be a string with < 8 characters
    //   0x2C "Flipper One"                               (11 chars, 6 rows), SCSI product must be a string with < 16 characters
    //   0x32 "1.00"                                      ( 4 chars, 2 rows), SCSI version string, max-length 4 ASCII chars
    //   0x34 "https://r.flipper.net/flipper_one_mcu_update" (44 chars, 22 rows), redirect URL, max-length 127 ASCII chars
    //   0x4A "How to Update Firmware"                    (22 chars, 11 rows), redirect name, max-length 127 ASCII chars
    //   0x55 "Flipper One"                               (11 chars, 6 rows), model string, max-length 127 ASCII chars
    //   0x5B "F0B0C1"                                    ( 6 chars, 3 rows), board_id string, max-length 127 ASCII chars

    // clang-format off
    const uint16_t wl_table[16] = {
        [0]  = 0x37C1,                       // VID
        [1]  = 0xF102,                       // PID
        [2]  = 0x0100,                       // bcdDevice 1.00
        [3]  = 0x0409,                       // lang_id (English US)
        [4]  = WL_STRDEF(0x10, 12),          // manufacturer "Flipper FZCO"
        [5]  = WL_STRDEF(0x16, 26),          // product "Flipper One MCU Bootloader"
        [6]  = 0x0000,                       // serial number string (not set)
        [7]  = (0x32 << 8) | 0xE0,          // bMaxPower=0x32 (100mA), bmAttributes=0xE0
        [8]  = WL_STRDEF(0x23, 10),          // volume label "FlipOneMCU"
        [9]  = WL_STRDEF(0x28,  7),          // SCSI vendor "Flipper"
        [10] = WL_STRDEF(0x2C, 11),          // SCSI product "Flipper One"
        [11] = WL_STRDEF(0x32,  4),          // SCSI version "1.00"
        [12] = WL_STRDEF(0x34, 44),          // redirect URL "https://r.flipper.net/flipper_one_mcu_update"
        [13] = WL_STRDEF(0x4A, 22),          // redirect name "How to Update Firmware"
        [14] = WL_STRDEF(0x55, 11),          // model "Flipper One"
        [15] = WL_STRDEF(0x5B,  6),          // board_id "F0B0C1"
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
    if(!furi_hal_otp_write_string(WL_BASE_ROW, 0x10, "Flipper FZCO")) return false;
    if(!furi_hal_otp_write_string(WL_BASE_ROW, 0x16, "Flipper One MCU Bootloader")) return false;
    if(!furi_hal_otp_write_string(WL_BASE_ROW, 0x23, "FlipOneMCU")) return false;
    if(!furi_hal_otp_write_string(WL_BASE_ROW, 0x28, "Flipper")) return false;
    if(!furi_hal_otp_write_string(WL_BASE_ROW, 0x2C, "Flipper One")) return false;
    if(!furi_hal_otp_write_string(WL_BASE_ROW, 0x32, "1.00")) return false;
    if(!furi_hal_otp_write_string(WL_BASE_ROW, 0x34, "https://r.flipper.net/flipper_one_mcu_update")) return false;
    if(!furi_hal_otp_write_string(WL_BASE_ROW, 0x4A, "How to Update Firmware")) return false;
    if(!furi_hal_otp_write_string(WL_BASE_ROW, 0x55, "Flipper One")) return false;
    if(!furi_hal_otp_write_string(WL_BASE_ROW, 0x5B, "F0B0C1")) return false;

    // 3. Write USB_WHITE_LABEL_ADDR (ECC) — points to WL_BASE_ROW
    if(!furi_hal_otp_write_ecc(OTP_DATA_USB_WHITE_LABEL_ADDR_ROW, WL_BASE_ROW)) return false;

    // 4. Write USB_BOOT_FLAGS (raw, 3× redundant) — mark all written entries valid
    const uint32_t usb_boot_flags =
        OTP_DATA_USB_BOOT_FLAGS_WHITE_LABEL_ADDR_VALID_BITS | OTP_DATA_USB_BOOT_FLAGS_WL_USB_DEVICE_VID_VALUE_VALID_BITS |
        OTP_DATA_USB_BOOT_FLAGS_WL_USB_DEVICE_PID_VALUE_VALID_BITS |
        OTP_DATA_USB_BOOT_FLAGS_WL_USB_DEVICE_SERIAL_NUMBER_VALUE_VALID_BITS | // entry 2: bcdDevice
        OTP_DATA_USB_BOOT_FLAGS_WL_USB_DEVICE_LANG_ID_VALUE_VALID_BITS | OTP_DATA_USB_BOOT_FLAGS_WL_USB_DEVICE_MANUFACTURER_STRDEF_VALID_BITS |
        OTP_DATA_USB_BOOT_FLAGS_WL_USB_DEVICE_PRODUCT_STRDEF_VALID_BITS |
        // bit 6 (SERIAL_NUMBER_STRDEF) intentionally not set — no serial string
        OTP_DATA_USB_BOOT_FLAGS_WL_USB_CONFIG_ATTRIBUTES_MAX_POWER_VALUES_VALID_BITS | OTP_DATA_USB_BOOT_FLAGS_WL_VOLUME_LABEL_STRDEF_VALID_BITS |
        OTP_DATA_USB_BOOT_FLAGS_WL_SCSI_INQUIRY_VENDOR_STRDEF_VALID_BITS | OTP_DATA_USB_BOOT_FLAGS_WL_SCSI_INQUIRY_PRODUCT_STRDEF_VALID_BITS |
        OTP_DATA_USB_BOOT_FLAGS_WL_SCSI_INQUIRY_VERSION_STRDEF_VALID_BITS | OTP_DATA_USB_BOOT_FLAGS_WL_INDEX_HTM_REDIRECT_URL_STRDEF_VALID_BITS |
        OTP_DATA_USB_BOOT_FLAGS_WL_INDEX_HTM_REDIRECT_NAME_STRDEF_VALID_BITS | OTP_DATA_USB_BOOT_FLAGS_WL_INFO_UF2_TXT_MODEL_STRDEF_VALID_BITS |
        OTP_DATA_USB_BOOT_FLAGS_WL_INFO_UF2_TXT_BOARD_ID_STRDEF_VALID_BITS;

    if(!furi_hal_otp_write_raw(OTP_DATA_USB_BOOT_FLAGS_ROW, usb_boot_flags)) return false;
    if(!furi_hal_otp_write_raw(OTP_DATA_USB_BOOT_FLAGS_R1_ROW, usb_boot_flags)) return false;
    if(!furi_hal_otp_write_raw(OTP_DATA_USB_BOOT_FLAGS_R2_ROW, usb_boot_flags)) return false;

    FURI_LOG_I(TAG, "USB white label written to OTP");
    return true;
}
