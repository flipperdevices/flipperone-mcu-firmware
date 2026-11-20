#include <furi.h>
#include <furi_hal_otp.h>
#include <furi_hal_version.h>
#include <pico/bootrom.h>
#include <hardware/structs/otp.h>
#include <hardware/regs/otp_data.h>
#include <string.h>

#define TAG "FuriHalOtp"

// Row in OTP where the USB white label table starts (must be page-aligned recommended)
#define WL_BASE_ROW 0x0400

// STRDEF encoding: high byte = row offset from WL_BASE_ROW, low byte = char count (ASCII)
#define WL_STRDEF(row_offset, char_count) (((row_offset) << 8) | (char_count))

// In the following macros, "" is used to ensure that the parameter is a string literal and not a pointer

// Helper macros to calculate string lengths and row counts at compile time
#define STR_LEN(str) (sizeof("" str) - 1)

// Number of ECC rows a string occupies: 2 ASCII chars per row, ceil(len/2) = sizeof(str)/2,
#define STR_ROWS(str) (sizeof("" str) / 2)

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
#define OTP_SCSI_PRODUCT     "One"         // max 16 chars
#define OTP_SCSI_VERSION     "1.00"        // max 4 chars

// BOOTSEL LED: GPIO pin, connected to VCC
#define OTP_BOOTSEL_LED_PIN  35

// Bootloader info page strings
#define OTP_REDIRECT_URL     "https://r.flipper.net/flipper_one_mcu_update" // max 127 chars
#define OTP_REDIRECT_NAME    "How to Update Firmware" // max 127 chars
#define OTP_MODEL            "Flipper One"            // max 127 chars
#define OTP_BOARD_ID_FORMAT         "F%luB%luC%lu"    // max 127 chars

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

// Device name: a STRDEF row (same encoding as the white label) followed by the
// string, 2 ASCII chars per ECC row. It starts on its own OTP page, after the
// rows the white label can use with the longest board ID, so the page can be
// locked separately.
#define DEVICE_NAME_BASE_ROW   0x04C0
#define DEVICE_NAME_STR_OFFSET 0x01
#define DEVICE_NAME_STR_ROWS   ((FURI_HAL_VERSION_NAME_LENGTH + 1) / 2)

static_assert(DEVICE_NAME_BASE_ROW % 0x40 == 0, "Device name must start an OTP page");
static_assert(DEVICE_NAME_BASE_ROW >= WL_BASE_ROW + WL_OFF_BOARD_ID + (127 + 1) / 2, "Device name overlaps the USB white label");

// clang-format on

void furi_hal_otp_init(void) {
    FURI_LOG_I(TAG, "OTP init");

    if(!furi_hal_otp_usb_white_label_valid()) {
        FURI_LOG_E(TAG, "USB white label is not valid");
    }

    if(!furi_hal_otp_bootsel_led_valid()) {
        FURI_LOG_E(TAG, "BOOTSEL LED is not valid");
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

bool furi_hal_otp_usb_white_label_valid(void) {
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

FuriHalOtpUsbWhiteLabelError furi_hal_otp_write_usb_white_label(uint32_t firmware_id, uint32_t body_id, uint32_t connectivity_id) {
    FuriString* board_id_str = furi_string_alloc_printf(OTP_BOARD_ID_FORMAT, firmware_id, body_id, connectivity_id);
    FuriHalOtpUsbWhiteLabelError error = FuriHalOtpUsbWhiteLabelErrorNone;

    do {
        if(furi_string_size(board_id_str) >= 127) {
            error = FuriHalOtpUsbWhiteLabelErrorBoardIdTooLong;
            break;
        }

        // USB white label table: 16 ECC rows at WL_BASE_ROW
        // String data follows the table, 2 ASCII chars per ECC row, packed little-endian

        // clang-format off
        const uint16_t wl_table[16] = {
            [0]  = OTP_USB_VID,                                                      // VID
            [1]  = OTP_USB_PID,                                                      // PID
            [2]  = OTP_USB_BCD,                                                      // bcdDevice 1.00
            [3]  = OTP_USB_LANG_ID,                                                  // lang_id (English US)
            [4]  = WL_STRDEF(WL_OFF_MANUFACTURER, STR_LEN(OTP_USB_MANUFACTURER)),    // manufacturer
            [5]  = WL_STRDEF(WL_OFF_PRODUCT,      STR_LEN(OTP_USB_PRODUCT)),         // product
            [6]  = 0x0000,                                                           // serial number string (not set)
            [7]  = (OTP_USB_MAX_POWER << 8) | OTP_USB_ATTRIBUTES,                    // bMaxPower, bmAttributes
            [8]  = WL_STRDEF(WL_OFF_VOLUME_LABEL,  STR_LEN(OTP_USB_VOLUME_LABEL)),   // volume label
            [9]  = WL_STRDEF(WL_OFF_SCSI_VENDOR,   STR_LEN(OTP_SCSI_VENDOR)),        // SCSI vendor
            [10] = WL_STRDEF(WL_OFF_SCSI_PRODUCT,  STR_LEN(OTP_SCSI_PRODUCT)),       // SCSI product
            [11] = WL_STRDEF(WL_OFF_SCSI_VERSION,  STR_LEN(OTP_SCSI_VERSION)),       // SCSI version
            [12] = WL_STRDEF(WL_OFF_REDIRECT_URL,  STR_LEN(OTP_REDIRECT_URL)),       // redirect URL
            [13] = WL_STRDEF(WL_OFF_REDIRECT_NAME, STR_LEN(OTP_REDIRECT_NAME)),      // redirect name
            [14] = WL_STRDEF(WL_OFF_MODEL,         STR_LEN(OTP_MODEL)),              // model
            [15] = WL_STRDEF(WL_OFF_BOARD_ID,      furi_string_size(board_id_str)),  // board_id
        };
        // clang-format on

        // 1. Write white label table (16 consecutive ECC rows)
        otp_cmd_t cmd = {.flags = WL_BASE_ROW | OTP_CMD_ECC_BITS | OTP_CMD_WRITE_BITS};
        int rc = rom_func_otp_access((uint8_t*)wl_table, sizeof(wl_table), cmd);
        if(rc != BOOTROM_OK) {
            FURI_LOG_E(TAG, "OTP white label table write failed: %d", rc);
            error = FuriHalOtpUsbWhiteLabelErrorWriteTableFailed;
            break;
        }

        // 2. Write string data
        if(!furi_hal_otp_usb_white_label_write_string(WL_BASE_ROW, WL_OFF_MANUFACTURER, OTP_USB_MANUFACTURER)) {
            error = FuriHalOtpUsbWhiteLabelErrorWriteManufacturerFailed;
            break;
        }
        if(!furi_hal_otp_usb_white_label_write_string(WL_BASE_ROW, WL_OFF_PRODUCT, OTP_USB_PRODUCT)) {
            error = FuriHalOtpUsbWhiteLabelErrorWriteProductFailed;
            break;
        }
        if(!furi_hal_otp_usb_white_label_write_string(WL_BASE_ROW, WL_OFF_VOLUME_LABEL, OTP_USB_VOLUME_LABEL)) {
            error = FuriHalOtpUsbWhiteLabelErrorWriteVolumeLabelFailed;
            break;
        }
        if(!furi_hal_otp_usb_white_label_write_string(WL_BASE_ROW, WL_OFF_SCSI_VENDOR, OTP_SCSI_VENDOR)) {
            error = FuriHalOtpUsbWhiteLabelErrorWriteScsiVendorFailed;
            break;
        }
        if(!furi_hal_otp_usb_white_label_write_string(WL_BASE_ROW, WL_OFF_SCSI_PRODUCT, OTP_SCSI_PRODUCT)) {
            error = FuriHalOtpUsbWhiteLabelErrorWriteScsiProductFailed;
            break;
        }
        if(!furi_hal_otp_usb_white_label_write_string(WL_BASE_ROW, WL_OFF_SCSI_VERSION, OTP_SCSI_VERSION)) {
            error = FuriHalOtpUsbWhiteLabelErrorWriteScsiVersionFailed;
            break;
        }
        if(!furi_hal_otp_usb_white_label_write_string(WL_BASE_ROW, WL_OFF_REDIRECT_URL, OTP_REDIRECT_URL)) {
            error = FuriHalOtpUsbWhiteLabelErrorWriteRedirectUrlFailed;
            break;
        }
        if(!furi_hal_otp_usb_white_label_write_string(WL_BASE_ROW, WL_OFF_REDIRECT_NAME, OTP_REDIRECT_NAME)) {
            error = FuriHalOtpUsbWhiteLabelErrorWriteRedirectNameFailed;
            break;
        }
        if(!furi_hal_otp_usb_white_label_write_string(WL_BASE_ROW, WL_OFF_MODEL, OTP_MODEL)) {
            error = FuriHalOtpUsbWhiteLabelErrorWriteModelFailed;
            break;
        }
        if(!furi_hal_otp_usb_white_label_write_string(WL_BASE_ROW, WL_OFF_BOARD_ID, furi_string_get_cstr(board_id_str))) {
            error = FuriHalOtpUsbWhiteLabelErrorWriteBoardIdFailed;
            break;
        }

        // 3. Write USB_WHITE_LABEL_ADDR (ECC) — points to WL_BASE_ROW
        if(!furi_hal_otp_write_ecc(OTP_DATA_USB_WHITE_LABEL_ADDR_ROW, WL_BASE_ROW)) {
            error = FuriHalOtpUsbWhiteLabelErrorWriteAddrFailed;
            break;
        }

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

        if(!furi_hal_otp_write_raw(OTP_DATA_USB_BOOT_FLAGS_ROW, usb_boot_flags)) {
            error = FuriHalOtpUsbWhiteLabelErrorWriteBootFlagsFailed;
            break;
        }
        if(!furi_hal_otp_write_raw(OTP_DATA_USB_BOOT_FLAGS_R1_ROW, usb_boot_flags)) {
            error = FuriHalOtpUsbWhiteLabelErrorWriteBootFlagsR1Failed;
            break;
        }
        if(!furi_hal_otp_write_raw(OTP_DATA_USB_BOOT_FLAGS_R2_ROW, usb_boot_flags)) {
            error = FuriHalOtpUsbWhiteLabelErrorWriteBootFlagsR2Failed;
            break;
        }
    } while(false);

    furi_string_free(board_id_str);

    return error;
}

bool furi_hal_otp_bootsel_led_valid(void) {
    // BOOT_FLAGS0 is a 3x redundant 24-bit row (no ECC)
    // Raw mode: 4 bytes per row, low 24 bits = OTP value
    uint32_t buf = 0;
    otp_cmd_t cmd = {.flags = OTP_DATA_BOOT_FLAGS0_ROW};
    int rc = rom_func_otp_access((uint8_t*)&buf, sizeof(buf), cmd);
    if(rc != BOOTROM_OK) {
        FURI_LOG_E(TAG, "OTP read failed: %d", rc);
        return false;
    }
    return (buf & OTP_DATA_BOOT_FLAGS0_ENABLE_BOOTSEL_LED_BITS) != 0;
}

bool furi_hal_otp_write_bootsel_led(void) {
    // 1. BOOTSEL_LED_CFG (ECC): pin number in bits 5:0, ACTIVELOW (bit 8) = 0 (errata https://forums.raspberrypi.com/viewtopic.php?t=397828)
    const uint16_t led_cfg = (OTP_BOOTSEL_LED_PIN << OTP_DATA_BOOTSEL_LED_CFG_PIN_LSB) & OTP_DATA_BOOTSEL_LED_CFG_PIN_BITS;
    if(!furi_hal_otp_write_ecc(OTP_DATA_BOOTSEL_LED_CFG_ROW, led_cfg)) {
        return false;
    }

    // 2. BOOT_FLAGS0 (raw, 3x redundant): set ENABLE_BOOTSEL_LED
    // Raw OTP writes can only set bits 0->1, so previously programmed flags are preserved
    const uint32_t boot_flags0 = OTP_DATA_BOOT_FLAGS0_ENABLE_BOOTSEL_LED_BITS;
    if(!furi_hal_otp_write_raw(OTP_DATA_BOOT_FLAGS0_ROW, boot_flags0)) {
        return false;
    }
    if(!furi_hal_otp_write_raw(OTP_DATA_BOOT_FLAGS0_R1_ROW, boot_flags0)) {
        return false;
    }
    if(!furi_hal_otp_write_raw(OTP_DATA_BOOT_FLAGS0_R2_ROW, boot_flags0)) {
        return false;
    }

    return true;
}

const char* furi_hal_otp_usb_white_label_error_to_string(FuriHalOtpUsbWhiteLabelError error) {
    switch(error) {
    case FuriHalOtpUsbWhiteLabelErrorNone:
        return "None";
    case FuriHalOtpUsbWhiteLabelErrorBoardIdTooLong:
        return "Board ID too long";
    case FuriHalOtpUsbWhiteLabelErrorWriteTableFailed:
        return "Write white label table failed";
    case FuriHalOtpUsbWhiteLabelErrorWriteManufacturerFailed:
        return "Write manufacturer string failed";
    case FuriHalOtpUsbWhiteLabelErrorWriteProductFailed:
        return "Write product string failed";
    case FuriHalOtpUsbWhiteLabelErrorWriteVolumeLabelFailed:
        return "Write volume label string failed";
    case FuriHalOtpUsbWhiteLabelErrorWriteScsiVendorFailed:
        return "Write SCSI vendor string failed";
    case FuriHalOtpUsbWhiteLabelErrorWriteScsiProductFailed:
        return "Write SCSI product string failed";
    case FuriHalOtpUsbWhiteLabelErrorWriteScsiVersionFailed:
        return "Write SCSI version string failed";
    case FuriHalOtpUsbWhiteLabelErrorWriteRedirectUrlFailed:
        return "Write redirect URL string failed";
    case FuriHalOtpUsbWhiteLabelErrorWriteRedirectNameFailed:
        return "Write redirect name string failed";
    case FuriHalOtpUsbWhiteLabelErrorWriteModelFailed:
        return "Write model string failed";
    case FuriHalOtpUsbWhiteLabelErrorWriteBoardIdFailed:
        return "Write board ID string failed";
    case FuriHalOtpUsbWhiteLabelErrorWriteAddrFailed:
        return "Write USB white label address failed";
    case FuriHalOtpUsbWhiteLabelErrorWriteBootFlagsFailed:
        return "Write USB boot flags failed";
    case FuriHalOtpUsbWhiteLabelErrorWriteBootFlagsR1Failed:
        return "Write USB boot flags R1 failed";
    case FuriHalOtpUsbWhiteLabelErrorWriteBootFlagsR2Failed:
        return "Write USB boot flags R2 failed";
    }
    return "Unknown error";
}

static bool furi_hal_otp_read_ecc(uint16_t row, uint16_t* buf, size_t rows) {
    otp_cmd_t cmd = {.flags = row | OTP_CMD_ECC_BITS};
    int rc = rom_func_otp_access((uint8_t*)buf, rows * sizeof(uint16_t), cmd);
    if(rc != BOOTROM_OK) {
        FURI_LOG_E(TAG, "OTP ECC read row 0x%04x failed: %d", row, rc);
        return false;
    }
    return true;
}

bool furi_hal_otp_device_name_is_valid(const char* name) {
    furi_check(name);

    size_t len = strlen(name);
    if(len == 0 || len > FURI_HAL_VERSION_NAME_LENGTH) {
        return false;
    }

    for(size_t i = 0; i < len; i++) {
        const char c = name[i];
        if(!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))) {
            return false;
        }
    }

    return true;
}

bool furi_hal_otp_read_device_name(char* name, size_t name_size) {
    furi_check(name);
    furi_check(name_size >= FURI_HAL_VERSION_ARRAY_NAME_LENGTH);

    name[0] = '\0';

    uint16_t strdef = 0;
    if(!furi_hal_otp_read_ecc(DEVICE_NAME_BASE_ROW, &strdef, 1) || strdef == 0) {
        return false;
    }

    const uint8_t row_offset = strdef >> 8;
    const uint8_t len = strdef & 0xFF;
    if(row_offset != DEVICE_NAME_STR_OFFSET || len == 0 || len > FURI_HAL_VERSION_NAME_LENGTH) {
        FURI_LOG_E(TAG, "Device name STRDEF 0x%04x is not valid", strdef);
        return false;
    }

    uint16_t buf[DEVICE_NAME_STR_ROWS] = {0};
    if(!furi_hal_otp_read_ecc(DEVICE_NAME_BASE_ROW + row_offset, buf, DEVICE_NAME_STR_ROWS)) {
        return false;
    }

    char chars[DEVICE_NAME_STR_ROWS * 2];
    for(uint8_t i = 0; i < sizeof(chars); i++) {
        chars[i] = (i & 1) ? (char)(buf[i / 2] >> 8) : (char)(buf[i / 2] & 0xFF);
    }

    // The rest of the rows must be blank, so that a STRDEF write cut short,
    // which can read back with a smaller length, doesn't give a shorter name
    for(uint8_t i = len; i < sizeof(chars); i++) {
        if(chars[i] != '\0') {
            FURI_LOG_E(TAG, "Device name in OTP is not valid");
            return false;
        }
    }

    memcpy(name, chars, len);
    name[len] = '\0';

    if(strlen(name) != len || !furi_hal_otp_device_name_is_valid(name)) {
        FURI_LOG_E(TAG, "Device name in OTP is not valid");
        name[0] = '\0';
        return false;
    }

    return true;
}

FuriHalOtpDeviceNameError furi_hal_otp_write_device_name(const char* name) {
    furi_check(name);

    if(!furi_hal_otp_device_name_is_valid(name)) {
        return FuriHalOtpDeviceNameErrorInvalidName;
    }

    uint16_t strdef = 0;
    if(!furi_hal_otp_read_ecc(DEVICE_NAME_BASE_ROW, &strdef, 1)) {
        return FuriHalOtpDeviceNameErrorReadFailed;
    }

    // Any value in the STRDEF row, even a broken one, means the rows were already used
    if(strdef != 0) {
        return FuriHalOtpDeviceNameErrorAlreadyProgrammed;
    }

    // String first, STRDEF last: the name only counts once it is complete. If the
    // write is cut short, writing the same name again succeeds, because OTP only
    // refuses writes that would clear bits.
    if(!furi_hal_otp_usb_white_label_write_string(DEVICE_NAME_BASE_ROW, DEVICE_NAME_STR_OFFSET, name)) {
        return FuriHalOtpDeviceNameErrorWriteNameFailed;
    }

    if(!furi_hal_otp_write_ecc(DEVICE_NAME_BASE_ROW, WL_STRDEF(DEVICE_NAME_STR_OFFSET, strlen(name)))) {
        return FuriHalOtpDeviceNameErrorWriteStrdefFailed;
    }

    // The bootrom doesn't check what got programmed, so read it back
    char check[FURI_HAL_VERSION_ARRAY_NAME_LENGTH];
    if(!furi_hal_otp_read_device_name(check, sizeof(check)) || strcmp(check, name) != 0) {
        return FuriHalOtpDeviceNameErrorVerifyFailed;
    }

    return FuriHalOtpDeviceNameErrorNone;
}

const char* furi_hal_otp_device_name_error_to_string(FuriHalOtpDeviceNameError error) {
    switch(error) {
    case FuriHalOtpDeviceNameErrorNone:
        return "None";
    case FuriHalOtpDeviceNameErrorInvalidName:
        return "Name must be 1 to " TOSTRING(FURI_HAL_VERSION_NAME_LENGTH) " ASCII letters or digits";
    case FuriHalOtpDeviceNameErrorReadFailed:
        return "Read device name STRDEF failed";
    case FuriHalOtpDeviceNameErrorAlreadyProgrammed:
        return "Device name is already programmed";
    case FuriHalOtpDeviceNameErrorWriteNameFailed:
        return "Write device name string failed";
    case FuriHalOtpDeviceNameErrorWriteStrdefFailed:
        return "Write device name STRDEF failed";
    case FuriHalOtpDeviceNameErrorVerifyFailed:
        return "Device name read back from OTP does not match";
    }
    return "Unknown error";
}
