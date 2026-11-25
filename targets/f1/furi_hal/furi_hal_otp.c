#include <furi.h>
#include <furi_hal_otp.h>
#include <pico/bootrom.h>
#include <hardware/structs/otp.h>
#include <hardware/regs/otp_data.h>
#include <string.h>

#define TAG "FuriHalOtp"

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

static bool furi_hal_otp_usb_white_label_lock(void);

// Two writers must not both find a block blank and write it
static FuriMutex* furi_hal_otp_write_mutex;

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

// clang-format on

void furi_hal_otp_init(void) {
    FURI_LOG_I(TAG, "OTP init");
    furi_hal_otp_write_mutex = furi_mutex_alloc(FuriMutexTypeNormal);

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

static int furi_hal_otp_write_ecc_rc(uint16_t row, uint16_t value) {
    otp_cmd_t cmd = {.flags = row | OTP_CMD_ECC_BITS | OTP_CMD_WRITE_BITS};
    return rom_func_otp_access((uint8_t*)&value, sizeof(value), cmd);
}

static bool furi_hal_otp_read_ecc(uint16_t row, uint16_t* value) {
    otp_cmd_t cmd = {.flags = row | OTP_CMD_ECC_BITS};
    return rom_func_otp_access((uint8_t*)value, sizeof(*value), cmd) == BOOTROM_OK;
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

// Copies of USB_BOOT_FLAGS that mark the white label
static uint32_t furi_hal_otp_usb_white_label_flag_copies(void) {
    uint32_t copies = 0;
    for(uint16_t row = OTP_DATA_USB_BOOT_FLAGS_ROW; row <= OTP_DATA_USB_BOOT_FLAGS_R2_ROW; row++) {
        uint32_t flags = 0;
        otp_cmd_t cmd = {.flags = row};
        if(rom_func_otp_access((uint8_t*)&flags, sizeof(flags), cmd) == BOOTROM_OK && (flags & OTP_DATA_USB_BOOT_FLAGS_WHITE_LABEL_ADDR_VALID_BITS)) {
            copies++;
        }
    }
    return copies;
}

bool furi_hal_otp_usb_white_label_valid(void) {
    // As the bootrom reads it: a majority of the three copies
    return furi_hal_otp_usb_white_label_flag_copies() >= 2;
}

static FuriHalOtpUsbWhiteLabelError furi_hal_otp_write_usb_white_label(uint32_t target, uint32_t body, uint32_t connect) {
    FuriString* board_id_str = furi_string_alloc_printf(OTP_BOARD_ID_FORMAT, target, body, connect);
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

        if(!furi_hal_otp_usb_white_label_lock()) {
            error = FuriHalOtpUsbWhiteLabelErrorLockFailed;
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

static const char* furi_hal_otp_usb_white_label_error_to_string(FuriHalOtpUsbWhiteLabelError error) {
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
    case FuriHalOtpUsbWhiteLabelErrorLockFailed:
        return "Lock white label pages failed";
    }
    return "Unknown error";
}

// Page locks

#define OTP_PAGE_ROWS NUM_OTP_PAGE_ROWS

// PAGEn_LOCK1: read only for Secure, Non-secure and bootloader access, in each
// of the row's three majority-vote copies
#define OTP_PAGE_LOCK1_READ_ONLY                                                          \
    ((OTP_DATA_PAGE0_LOCK1_LOCK_S_VALUE_READ_ONLY << OTP_DATA_PAGE0_LOCK1_LOCK_S_LSB) |   \
     (OTP_DATA_PAGE0_LOCK1_LOCK_NS_VALUE_READ_ONLY << OTP_DATA_PAGE0_LOCK1_LOCK_NS_LSB) | \
     (OTP_DATA_PAGE0_LOCK1_LOCK_BL_VALUE_READ_ONLY << OTP_DATA_PAGE0_LOCK1_LOCK_BL_LSB))
#define OTP_PAGE_LOCK1_ROW_VALUE (OTP_PAGE_LOCK1_READ_ONLY * 0x010101u)

static bool furi_hal_otp_read_raw(uint16_t row, uint32_t* value) {
    otp_cmd_t cmd = {.flags = row};
    int rc = rom_func_otp_access((uint8_t*)value, sizeof(*value), cmd);
    if(rc != BOOTROM_OK) {
        FURI_LOG_E(TAG, "OTP raw read row 0x%04x failed: %d", row, rc);
        return false;
    }
    *value &= 0xFFFFFF;
    return true;
}

static bool furi_hal_otp_lock_page(uint32_t page) {
    furi_check(page < NUM_OTP_PAGES);
    // Permanent from the next reset. Skipped once the row holds the lock; a
    // row cut short is completed, as writing only adds bits.
    const uint16_t row = OTP_DATA_PAGE0_LOCK1_ROW + page * (OTP_DATA_PAGE1_LOCK1_ROW - OTP_DATA_PAGE0_LOCK1_ROW);
    uint32_t lock = 0;
    if(!furi_hal_otp_read_raw(row, &lock)) return false;
    if(lock != OTP_PAGE_LOCK1_ROW_VALUE && !furi_hal_otp_write_raw(row, OTP_PAGE_LOCK1_ROW_VALUE)) {
        return false;
    }
    // And for this boot
    otp_hw->sw_lock[page] = (OTP_SW_LOCK0_SEC_VALUE_READ_ONLY << OTP_SW_LOCK0_SEC_LSB) | (OTP_SW_LOCK0_NSEC_VALUE_READ_ONLY << OTP_SW_LOCK0_NSEC_LSB);
    return true;
}

static bool furi_hal_otp_lock_rows(uint32_t first_row, uint32_t row_count) {
    for(uint32_t page = first_row / OTP_PAGE_ROWS; page <= (first_row + row_count - 1) / OTP_PAGE_ROWS; page++) {
        if(!furi_hal_otp_lock_page(page)) {
            FURI_LOG_E(TAG, "Locking OTP page %lu failed", page);
            return false;
        }
    }
    return true;
}

// The white label and the longest board id OTP_BOARD_ID_FORMAT makes:
// "F%luB%luC%lu" with 10 digits each is 33 chars, 17 rows
#define WL_ROWS            (WL_OFF_BOARD_ID + 17)
// Table entry with the board id's STRDEF
#define WL_BOARD_ID_STRDEF 15
static_assert(WL_BASE_ROW % OTP_PAGE_ROWS + WL_ROWS <= 2 * OTP_PAGE_ROWS, "USB white label outgrows pages 16..17");

static bool furi_hal_otp_usb_white_label_lock(void) {
    return furi_hal_otp_lock_rows(WL_BASE_ROW, WL_ROWS);
}

// Whether the written white label's board id is the one of these values
static bool furi_hal_otp_usb_white_label_board_id_is(uint32_t target, uint32_t body, uint32_t connect) {
    char want[34];
    snprintf(want, sizeof(want), OTP_BOARD_ID_FORMAT, target, body, connect);

    uint16_t strdef = 0;
    otp_cmd_t cmd = {.flags = (WL_BASE_ROW + WL_BOARD_ID_STRDEF) | OTP_CMD_ECC_BITS};
    if(rom_func_otp_access((uint8_t*)&strdef, sizeof(strdef), cmd) != BOOTROM_OK) return false;
    const size_t len = strdef & 0xFF;
    if(len != strlen(want)) return false;

    uint16_t rows[(sizeof(want) + 1) / 2] = {0};
    cmd.flags = (WL_BASE_ROW + (strdef >> 8)) | OTP_CMD_ECC_BITS;
    if(rom_func_otp_access((uint8_t*)rows, (len + 1) / 2 * sizeof(uint16_t), cmd) != BOOTROM_OK) return false;
    return memcmp(rows, want, len) == 0;
}

// OTP_DEVICE and OTP_MAC blocks

#define OTP_BLOCK_HEADER_ROWS  2
#define OTP_BLOCK_PAYLOAD_SIZE 48
#define OTP_BLOCK_ROWS         (OTP_BLOCK_HEADER_ROWS + OTP_BLOCK_PAYLOAD_SIZE / 2)

static_assert(FURI_HAL_OTP_DEVICE_NAME_SIZE + 4 * 2 + 8 == OTP_BLOCK_PAYLOAD_SIZE, "OTP_DEVICE payload size");
static_assert(FuriHalOtpMacCount * FURI_HAL_OTP_MAC_SIZE == OTP_BLOCK_PAYLOAD_SIZE, "OTP_MAC payload size");
static_assert(FURI_HAL_OTP_DEVICE_ROW / OTP_PAGE_ROWS != FURI_HAL_OTP_MAC_ROW / OTP_PAGE_ROWS, "OTP blocks share a page");
static_assert(FURI_HAL_OTP_DEVICE_ROW % OTP_PAGE_ROWS + OTP_BLOCK_ROWS <= OTP_PAGE_ROWS, "OTP_DEVICE spans pages");
static_assert(FURI_HAL_OTP_MAC_ROW % OTP_PAGE_ROWS + OTP_BLOCK_ROWS <= OTP_PAGE_ROWS, "OTP_MAC spans pages");

static bool furi_hal_otp_block_read_rows(uint16_t base_row, uint16_t rows[OTP_BLOCK_ROWS]) {
    otp_cmd_t cmd = {.flags = base_row | OTP_CMD_ECC_BITS};
    int rc = rom_func_otp_access((uint8_t*)rows, OTP_BLOCK_ROWS * sizeof(uint16_t), cmd);
    if(rc != BOOTROM_OK) {
        FURI_LOG_E(TAG, "OTP block read at 0x%04x failed: %d", base_row, rc);
        return false;
    }
    return true;
}

// Payload of a block of a known layout, or false
static bool furi_hal_otp_block_read(uint16_t base_row, uint16_t magic, uint8_t payload[OTP_BLOCK_PAYLOAD_SIZE]) {
    uint16_t rows[OTP_BLOCK_ROWS];
    memset(payload, 0, OTP_BLOCK_PAYLOAD_SIZE);
    // A blank device reads no ECC rows at all: the magic is written last
    uint32_t magic_raw = 0;
    if(!furi_hal_otp_read_raw(base_row, &magic_raw) || !magic_raw) return false;
    if(!furi_hal_otp_block_read_rows(base_row, rows)) return false;
    if(rows[0] != magic) return false;
    if(rows[1] != FURI_HAL_OTP_LAYOUT_VERSION) {
        FURI_LOG_E(TAG, "OTP block at 0x%04x: unknown layout %u", base_row, rows[1]);
        return false;
    }
    memcpy(payload, &rows[OTP_BLOCK_HEADER_ROWS], OTP_BLOCK_PAYLOAD_SIZE);
    return true;
}

// Make the block hold this payload, the magic last, and lock its page. A
// write cut short, even inside a row, is finished by the same call with the
// same data: writing a row again only adds bits, and the bootrom refuses a
// write that would clear one, which is how a row holding other data shows.
static FuriHalOtpError furi_hal_otp_block_write(uint16_t base_row, uint16_t magic, const uint8_t payload[OTP_BLOCK_PAYLOAD_SIZE]) {
    uint16_t want[OTP_BLOCK_ROWS] = {magic, FURI_HAL_OTP_LAYOUT_VERSION};
    memcpy(&want[OTP_BLOCK_HEADER_ROWS], payload, OTP_BLOCK_PAYLOAD_SIZE);

    // Row by row: one row cut short must not hide the others
    bool complete = true;
    for(size_t i = 0; i < OTP_BLOCK_ROWS && complete; i++) {
        uint16_t have = 0;
        complete = furi_hal_otp_read_ecc(base_row + i, &have) && have == want[i];
    }

    if(!complete) {
        // A finished block with other data, its page likely locked: leave it
        uint16_t have_magic = 0;
        if(furi_hal_otp_read_ecc(base_row, &have_magic) && have_magic == magic) return FuriHalOtpErrorAlreadyWritten;

        for(size_t i = 1; i <= OTP_BLOCK_ROWS; i++) {
            const uint16_t row = base_row + i % OTP_BLOCK_ROWS; // the magic, row 0, last
            const uint16_t value = want[i % OTP_BLOCK_ROWS];
            if(!value) {
                // Nothing to program; any bit there is someone else's
                uint32_t raw = 0;
                if(!furi_hal_otp_read_raw(row, &raw)) return FuriHalOtpErrorWriteFailed;
                if(raw) return FuriHalOtpErrorNotBlank;
                continue;
            }
            int rc = furi_hal_otp_write_ecc_rc(row, value);
            if(rc == BOOTROM_ERROR_UNSUPPORTED_MODIFICATION) return FuriHalOtpErrorNotBlank;
            if(rc != BOOTROM_OK) {
                FURI_LOG_E(TAG, "OTP ECC write row 0x%04x failed: %d", row, rc);
                return FuriHalOtpErrorWriteFailed;
            }
        }
    }

    if(!furi_hal_otp_lock_rows(base_row, OTP_BLOCK_ROWS)) return FuriHalOtpErrorLockFailed;
    return FuriHalOtpErrorNone;
}

const char* furi_hal_otp_error_to_string(FuriHalOtpError error) {
    switch(error) {
    case FuriHalOtpErrorNone:
        return "None";
    case FuriHalOtpErrorInvalidArg:
        return "Invalid value";
    case FuriHalOtpErrorAlreadyWritten:
        return "Already written with other data";
    case FuriHalOtpErrorNotBlank:
        return "Holds other data from a write cut short";
    case FuriHalOtpErrorWriteFailed:
        return "Write failed";
    case FuriHalOtpErrorLockFailed:
        return "Written, but locking its page failed";
    case FuriHalOtpErrorWhiteLabelFailed:
        return "Device written, USB white label failed";
    case FuriHalOtpErrorWhiteLabelMismatch:
        return "The USB white label holds another board id";
    }
    return "Unknown error";
}

// OTP_DEVICE

bool furi_hal_otp_device_valid(void) {
    uint8_t payload[OTP_BLOCK_PAYLOAD_SIZE];
    return furi_hal_otp_block_read(FURI_HAL_OTP_DEVICE_ROW, FURI_HAL_OTP_DEVICE_MAGIC, payload);
}

bool furi_hal_otp_device_read(FuriHalOtpDevice* device) {
    furi_check(device);
    uint8_t payload[OTP_BLOCK_PAYLOAD_SIZE];
    memset(device, 0, sizeof(*device));
    if(!furi_hal_otp_block_read(FURI_HAL_OTP_DEVICE_ROW, FURI_HAL_OTP_DEVICE_MAGIC, payload)) return false;

    const uint8_t* p = payload;
    memcpy(device->name, p, FURI_HAL_OTP_DEVICE_NAME_SIZE);
    device->name[FURI_HAL_OTP_DEVICE_NAME_SIZE - 1] = '\0';
    p += FURI_HAL_OTP_DEVICE_NAME_SIZE;
    memcpy(&device->version, p, 2);
    memcpy(&device->target, p + 2, 2);
    memcpy(&device->body, p + 4, 2);
    memcpy(&device->connect, p + 6, 2);
    memcpy(&device->timestamp, p + 8, 8);
    return true;
}

static FuriHalOtpError furi_hal_otp_device_write_locked(const FuriHalOtpDevice* device) {
    furi_check(device);
    if(strnlen(device->name, FURI_HAL_OTP_DEVICE_NAME_SIZE) >= FURI_HAL_OTP_DEVICE_NAME_SIZE) {
        return FuriHalOtpErrorInvalidArg;
    }

    uint8_t payload[OTP_BLOCK_PAYLOAD_SIZE] = {0};
    uint8_t* p = payload;
    memcpy(p, device->name, strlen(device->name)); // the rest stays 0
    p += FURI_HAL_OTP_DEVICE_NAME_SIZE;
    memcpy(p, &device->version, 2);
    memcpy(p + 2, &device->target, 2);
    memcpy(p + 4, &device->body, 2);
    memcpy(p + 6, &device->connect, 2);
    memcpy(p + 8, &device->timestamp, 8);

    // The white label carries the same board id. One started earlier, finished
    // or cut short, with another is refused before anything is written.
    uint32_t board_id_strdef = 0;
    if(!furi_hal_otp_read_raw(WL_BASE_ROW + WL_BOARD_ID_STRDEF, &board_id_strdef)) return FuriHalOtpErrorWriteFailed;
    if(board_id_strdef && !furi_hal_otp_usb_white_label_board_id_is(device->target, device->body, device->connect)) {
        return FuriHalOtpErrorWhiteLabelMismatch;
    }

    FuriHalOtpError error = furi_hal_otp_block_write(FURI_HAL_OTP_DEVICE_ROW, FURI_HAL_OTP_DEVICE_MAGIC, payload);
    if(error != FuriHalOtpErrorNone) return error;

    // A white label written before the locks existed is locked now. One cut
    // short is written again: it only adds what is missing.
    if(furi_hal_otp_usb_white_label_flag_copies() == 3) {
        return furi_hal_otp_usb_white_label_lock() ? FuriHalOtpErrorNone : FuriHalOtpErrorWhiteLabelFailed;
    }
    FuriHalOtpUsbWhiteLabelError wl_error = furi_hal_otp_write_usb_white_label(device->target, device->body, device->connect);
    if(wl_error != FuriHalOtpUsbWhiteLabelErrorNone) {
        FURI_LOG_E(TAG, "USB white label: %s", furi_hal_otp_usb_white_label_error_to_string(wl_error));
        return FuriHalOtpErrorWhiteLabelFailed;
    }
    return FuriHalOtpErrorNone;
}

// OTP_MAC

const char* furi_hal_otp_mac_name(FuriHalOtpMac mac) {
    static const char* const names[FuriHalOtpMacCount] = {
        [FuriHalOtpMacEth0] = "eth0",
        [FuriHalOtpMacEth1] = "eth1",
        [FuriHalOtpMacUsb] = "usb",
        [FuriHalOtpMacWifi24Sta] = "wifi_2g4_sta",
        [FuriHalOtpMacWifi5Sta] = "wifi_5g_sta",
        [FuriHalOtpMacWifi24Ap] = "wifi_2g4_ap",
        [FuriHalOtpMacWifi5Ap] = "wifi_5g_ap",
        [FuriHalOtpMacBt] = "bt",
    };
    furi_check(mac < FuriHalOtpMacCount);
    return names[mac];
}

bool furi_hal_otp_mac_valid(void) {
    uint8_t payload[OTP_BLOCK_PAYLOAD_SIZE];
    return furi_hal_otp_block_read(FURI_HAL_OTP_MAC_ROW, FURI_HAL_OTP_MAC_MAGIC, payload);
}

bool furi_hal_otp_mac_read(FuriHalOtpMacs* macs) {
    furi_check(macs);
    static_assert(sizeof(macs->mac) == OTP_BLOCK_PAYLOAD_SIZE, "OTP_MAC payload size");
    return furi_hal_otp_block_read(FURI_HAL_OTP_MAC_ROW, FURI_HAL_OTP_MAC_MAGIC, (uint8_t*)macs->mac);
}

static FuriHalOtpError furi_hal_otp_mac_write_locked(const FuriHalOtpMacs* macs) {
    furi_check(macs);
    return furi_hal_otp_block_write(FURI_HAL_OTP_MAC_ROW, FURI_HAL_OTP_MAC_MAGIC, (const uint8_t*)macs->mac);
}

FuriHalOtpError furi_hal_otp_device_write(const FuriHalOtpDevice* device) {
    furi_check(furi_mutex_acquire(furi_hal_otp_write_mutex, FuriWaitForever) == FuriStatusOk);
    FuriHalOtpError error = furi_hal_otp_device_write_locked(device);
    furi_mutex_release(furi_hal_otp_write_mutex);
    return error;
}

FuriHalOtpError furi_hal_otp_mac_write(const FuriHalOtpMacs* macs) {
    furi_check(furi_mutex_acquire(furi_hal_otp_write_mutex, FuriWaitForever) == FuriStatusOk);
    FuriHalOtpError error = furi_hal_otp_mac_write_locked(macs);
    furi_mutex_release(furi_hal_otp_write_mutex);
    return error;
}
