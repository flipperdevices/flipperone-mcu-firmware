#include <furi.h>
#include <furi_hal_otp.h>
#include <pico/bootrom.h>
#include <hardware/structs/otp.h>
#include <hardware/regs/otp_data.h>
#include <string.h>
#include <pico/unique_id.h>
#include <pico/sha256.h>
#include <uECC.h>

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
    FuriHalOtpUsbWhiteLabelErrorVerifyFailed,
    FuriHalOtpUsbWhiteLabelErrorLockFailed,
} FuriHalOtpUsbWhiteLabelError;

static bool furi_hal_otp_usb_white_label_lock(void);

// Every bootrom OTP call goes through it: the bootrom takes its OTP lock
// without waiting, and a write leaves OTP unreadable until it is done. Held
// across a whole block write too, so two writers cannot both find it blank.
static FuriMutex* furi_hal_otp_mutex;

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
    furi_hal_otp_mutex = furi_mutex_alloc(FuriMutexTypeRecursive);

    if(!furi_hal_otp_usb_white_label_valid()) {
        FURI_LOG_E(TAG, "USB white label is not valid");
    }

    if(!furi_hal_otp_bootsel_led_valid()) {
        FURI_LOG_E(TAG, "BOOTSEL LED is not valid");
    }
}

static void furi_hal_otp_acquire(void) {
    // Before the scheduler runs there is one thread
    if(furi_kernel_is_running()) furi_check(furi_mutex_acquire(furi_hal_otp_mutex, FuriWaitForever) == FuriStatusOk);
}

static void furi_hal_otp_release(void) {
    if(furi_kernel_is_running()) furi_mutex_release(furi_hal_otp_mutex);
}

static int furi_hal_otp_access(uint8_t* buf, uint32_t len, otp_cmd_t cmd) {
    furi_hal_otp_acquire();
    int rc = rom_func_otp_access(buf, len, cmd);
    furi_hal_otp_release();
    return rc;
}

static bool furi_hal_otp_read_raw(uint16_t row, uint32_t* value) {
    otp_cmd_t cmd = {.flags = row};
    int rc = furi_hal_otp_access((uint8_t*)value, sizeof(*value), cmd);
    if(rc != BOOTROM_OK) {
        FURI_LOG_E(TAG, "OTP raw read row 0x%04x failed: %d", row, rc);
        return false;
    }
    *value &= 0xFFFFFF;
    return true;
}

// An ECC row as the bootrom writes it: 16 data bits and 6 parity bits (from
// pico-bootrom-rp2350 src/main/arm/varm_otp.c, s_otp_calculate_ecc,
// BSD-3-Clause). Where set bits do not fit, the bootrom writes the row
// inverted with bits 23:22 set instead (bit repair by polarity).
static uint32_t furi_hal_otp_ecc(uint16_t value) {
    static const uint32_t parity[6] = {0x0AD5B, 0x0366D, 0x0C78E, 0x007F0, 0x0F800, 0x1FFFFF};
    uint32_t row = value;
    for(size_t i = 0; i < COUNT_OF(parity); i++) {
        row |= (uint32_t)__builtin_parity(row & parity[i]) << (16 + i);
    }
    return row;
}

#define OTP_ECC_BRP(row) (0xC00000u | ((row) ^ 0x3FFFFFu))

typedef enum {
    FuriHalOtpRowDone, // holds the wanted value
    FuriHalOtpRowToWrite, // blank, or a write of the wanted value cut short: writing completes it
    FuriHalOtpRowOther, // holds bits the wanted value has not
} FuriHalOtpRowState;

// What an ECC row holds against a wanted value, from its raw bits alone: no
// ECC read of a row that may not decode
static bool furi_hal_otp_row_state(uint16_t row, uint16_t want, FuriHalOtpRowState* state) {
    uint32_t raw = 0;
    if(!furi_hal_otp_read_raw(row, &raw)) return false;
    const uint32_t ecc = furi_hal_otp_ecc(want);
    if(raw == ecc || raw == OTP_ECC_BRP(ecc)) {
        *state = FuriHalOtpRowDone;
    } else if(!(raw & ~ecc)) {
        *state = FuriHalOtpRowToWrite;
    } else {
        *state = FuriHalOtpRowOther;
    }
    return true;
}

typedef struct {
    bool done; // every row holds its wanted value
    bool other; // some row holds bits its wanted value has not
} FuriHalOtpRows;

static bool furi_hal_otp_rows_scan(FuriHalOtpRows* rows, uint16_t first_row, const uint16_t* want, size_t count) {
    for(size_t i = 0; i < count; i++) {
        FuriHalOtpRowState state;
        if(!furi_hal_otp_row_state(first_row + i, want[i], &state)) return false;
        if(state != FuriHalOtpRowDone) rows->done = false;
        if(state == FuriHalOtpRowOther) rows->other = true;
    }
    return true;
}

// Whether count rows from first_row all hold their wanted values
static bool furi_hal_otp_rows_done(uint16_t first_row, const uint16_t* want, size_t count) {
    FuriHalOtpRows rows = {.done = true};
    return furi_hal_otp_rows_scan(&rows, first_row, want, count) && rows.done;
}

static bool furi_hal_otp_write_ecc(uint16_t row, uint16_t value) {
    otp_cmd_t cmd = {.flags = row | OTP_CMD_ECC_BITS | OTP_CMD_WRITE_BITS};
    int rc = furi_hal_otp_access((uint8_t*)&value, sizeof(value), cmd);
    if(rc != BOOTROM_OK) {
        FURI_LOG_E(TAG, "OTP ECC write row 0x%04x failed: %d", row, rc);
        return false;
    }
    return true;
}

static int furi_hal_otp_write_ecc_rc(uint16_t row, uint16_t value) {
    otp_cmd_t cmd = {.flags = row | OTP_CMD_ECC_BITS | OTP_CMD_WRITE_BITS};
    return furi_hal_otp_access((uint8_t*)&value, sizeof(value), cmd);
}

static bool furi_hal_otp_write_raw(uint16_t row, uint32_t value) {
    // The bootrom refuses a raw value that lacks a bit the row has
    uint32_t have = 0;
    if(!furi_hal_otp_read_raw(row, &have)) return false;
    value |= have;
    if(value == have) return true;
    otp_cmd_t cmd = {.flags = row | OTP_CMD_WRITE_BITS};
    int rc = furi_hal_otp_access((uint8_t*)&value, sizeof(value), cmd);
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
    int rc = furi_hal_otp_access((uint8_t*)buf, sizeof(buf), cmd);
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
        if(furi_hal_otp_access((uint8_t*)&flags, sizeof(flags), cmd) == BOOTROM_OK && (flags & OTP_DATA_USB_BOOT_FLAGS_WHITE_LABEL_ADDR_VALID_BITS)) {
            copies++;
        }
    }
    return copies;
}

bool furi_hal_otp_usb_white_label_valid(void) {
    // As the bootrom reads it: a majority of the three copies
    return furi_hal_otp_usb_white_label_flag_copies() >= 2;
}

// USB white label table: 16 ECC rows at WL_BASE_ROW; the strings follow it,
// 2 ASCII chars per ECC row, packed little-endian
static void furi_hal_otp_usb_white_label_table(uint16_t wl_table[16], size_t board_id_len) {
    // clang-format off
    const uint16_t table[16] = {
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
        [15] = WL_STRDEF(WL_OFF_BOARD_ID,      board_id_len),  // board_id
    };
    // clang-format on
    memcpy(wl_table, table, sizeof(table));
}

// The written white label, read back
static bool furi_hal_otp_usb_white_label_string_ok(uint8_t row_offset, const char* str) {
    uint16_t want[64] = {0};
    const size_t len = strlen(str);
    memcpy(want, str, len);
    return furi_hal_otp_rows_done(WL_BASE_ROW + row_offset, want, (len + 1) / 2);
}

static bool furi_hal_otp_usb_white_label_reads_back(const char* board_id) {
    uint16_t wl_table[16];
    furi_hal_otp_usb_white_label_table(wl_table, strlen(board_id));
    const uint16_t addr = WL_BASE_ROW;
    return furi_hal_otp_rows_done(WL_BASE_ROW, wl_table, 16) && furi_hal_otp_usb_white_label_string_ok(WL_OFF_MANUFACTURER, OTP_USB_MANUFACTURER) &&
           furi_hal_otp_usb_white_label_string_ok(WL_OFF_PRODUCT, OTP_USB_PRODUCT) &&
           furi_hal_otp_usb_white_label_string_ok(WL_OFF_VOLUME_LABEL, OTP_USB_VOLUME_LABEL) &&
           furi_hal_otp_usb_white_label_string_ok(WL_OFF_SCSI_VENDOR, OTP_SCSI_VENDOR) &&
           furi_hal_otp_usb_white_label_string_ok(WL_OFF_SCSI_PRODUCT, OTP_SCSI_PRODUCT) &&
           furi_hal_otp_usb_white_label_string_ok(WL_OFF_SCSI_VERSION, OTP_SCSI_VERSION) &&
           furi_hal_otp_usb_white_label_string_ok(WL_OFF_REDIRECT_URL, OTP_REDIRECT_URL) &&
           furi_hal_otp_usb_white_label_string_ok(WL_OFF_REDIRECT_NAME, OTP_REDIRECT_NAME) && furi_hal_otp_usb_white_label_string_ok(WL_OFF_MODEL, OTP_MODEL) &&
           furi_hal_otp_usb_white_label_string_ok(WL_OFF_BOARD_ID, board_id) && furi_hal_otp_rows_done(OTP_DATA_USB_WHITE_LABEL_ADDR_ROW, &addr, 1) &&
           furi_hal_otp_usb_white_label_flag_copies() == 3;
}

static FuriHalOtpUsbWhiteLabelError furi_hal_otp_write_usb_white_label(uint32_t target, uint32_t body, uint32_t connect) {
    FuriString* board_id_str = furi_string_alloc_printf(OTP_BOARD_ID_FORMAT, target, body, connect);
    FuriHalOtpUsbWhiteLabelError error = FuriHalOtpUsbWhiteLabelErrorNone;

    do {
        if(furi_string_size(board_id_str) >= 127) {
            error = FuriHalOtpUsbWhiteLabelErrorBoardIdTooLong;
            break;
        }

        uint16_t wl_table[16];
        furi_hal_otp_usb_white_label_table(wl_table, furi_string_size(board_id_str));

        // 1. Write white label table (16 consecutive ECC rows)
        otp_cmd_t cmd = {.flags = WL_BASE_ROW | OTP_CMD_ECC_BITS | OTP_CMD_WRITE_BITS};
        int rc = furi_hal_otp_access((uint8_t*)wl_table, sizeof(wl_table), cmd);
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

        // The bootrom does not check what it wrote: lock only what reads back right
        if(!furi_hal_otp_usb_white_label_reads_back(furi_string_get_cstr(board_id_str))) {
            error = FuriHalOtpUsbWhiteLabelErrorVerifyFailed;
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
    int rc = furi_hal_otp_access((uint8_t*)&buf, sizeof(buf), cmd);
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
    // Raw writes keep the flags already set
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
    case FuriHalOtpUsbWhiteLabelErrorVerifyFailed:
        return "White label reads back wrong";
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

static bool furi_hal_otp_lock_page(uint32_t page) {
    furi_check(page < NUM_OTP_PAGES);
    // Permanent from the next reset. A lock row cut short is completed, unless
    // the copies already set locked the page, its own lock row included, at a
    // reset: then it stays locked for Secure code only.
    const uint16_t row = OTP_DATA_PAGE0_LOCK1_ROW + page * (OTP_DATA_PAGE1_LOCK1_ROW - OTP_DATA_PAGE0_LOCK1_ROW);
    const bool written = furi_hal_otp_write_raw(row, OTP_PAGE_LOCK1_ROW_VALUE);
    // The bootrom does not check what it wrote. Each bit as the hardware
    // takes it: two of its three copies.
    uint32_t lock = 0;
    if(!furi_hal_otp_read_raw(row, &lock)) return false;
    const uint32_t a = lock & 0xFF, b = (lock >> 8) & 0xFF, c = (lock >> 16) & 0xFF;
    if(((a & b) | (a & c) | (b & c)) != OTP_PAGE_LOCK1_READ_ONLY) {
        FURI_LOG_E(TAG, "OTP page %lu: %s", page, written ? "lock row reads back wrong" : "locked for Secure code only");
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

// Whether a white label, finished or cut short, has another board id than
// these values: its board id STRDEF and string rows are compared with them
static bool furi_hal_otp_usb_white_label_board_id_differs(uint32_t target, uint32_t body, uint32_t connect) {
    char board_id[34];
    const size_t len = snprintf(board_id, sizeof(board_id), OTP_BOARD_ID_FORMAT, target, body, connect);
    uint16_t want[(sizeof(board_id) + 1) / 2] = {0};
    memcpy(want, board_id, len);
    const uint16_t strdef = WL_STRDEF(WL_OFF_BOARD_ID, len);

    FuriHalOtpRows rows = {.done = true};
    if(!furi_hal_otp_rows_scan(&rows, WL_BASE_ROW + WL_BOARD_ID_STRDEF, &strdef, 1) ||
       !furi_hal_otp_rows_scan(&rows, WL_BASE_ROW + WL_OFF_BOARD_ID, want, (len + 1) / 2)) {
        return true;
    }
    // A finished one must hold it all; one cut short only nothing else
    return rows.other || (furi_hal_otp_usb_white_label_valid() && !rows.done);
}

// Blocks 1..3

typedef struct {
    uint16_t base_row;
    uint16_t version;
    size_t size;
} FuriHalOtpBlockLayout;

static const FuriHalOtpBlockLayout furi_hal_otp_blocks[FuriHalOtpBlockCount + 1] = {
    [FuriHalOtpBlock1] = {0x0C0, 1, sizeof(FuriHalOtpBlock1Data)},
    [FuriHalOtpBlock2] = {0x100, 1, sizeof(FuriHalOtpBlock2Data)},
    [FuriHalOtpBlock3] = {0x180, 1, sizeof(FuriHalOtpBlock3Data)},
};

static_assert(sizeof(FuriHalOtpHeader) == 16, "OTP header size");
static_assert(sizeof(FuriHalOtpBlock1Data) == 34, "OTP block 1 size");
static_assert(sizeof(FuriHalOtpBlock2Data) == 160, "OTP block 2 size");
static_assert(sizeof(FuriHalOtpBlock3Data) == 148, "OTP block 3 size");
static_assert(0x0C0 + sizeof(FuriHalOtpBlock1Data) / 2 <= 0x100, "OTP block 1 outgrows page 3");
static_assert(0x100 + sizeof(FuriHalOtpBlock2Data) / 2 <= 0x180, "OTP block 2 outgrows pages 4..5");
static_assert(0x180 + sizeof(FuriHalOtpBlock3Data) / 2 <= 0x200, "OTP block 3 outgrows pages 6..7");
static_assert(PICO_UNIQUE_BOARD_ID_SIZE_BYTES == FURI_HAL_OTP_CHIP_ID_SIZE, "chip id size");

// Public halves of the production keys that sign block 3, by key id
static const uint8_t furi_hal_otp_keys[][FURI_HAL_OTP_SIGN_SIZE] = {
    // 0: a TEST key for development. Its private half is not kept secret:
    // a device must not trust it once production keys are here.
    {
        0xb3, 0xcb, 0x67, 0x31, 0x91, 0x32, 0xf9, 0x16, 0x3f, 0x1e, 0x9e, 0x1c, 0x7d, 0x84, 0x47, 0xc1, 0x47, 0x8d, 0x17, 0xe4, 0x99, 0x3a,
        0x34, 0xa1, 0x56, 0x51, 0xe4, 0x2c, 0x9d, 0x8a, 0x48, 0x17, 0x2d, 0xe4, 0x3d, 0x6f, 0xcf, 0x7a, 0x3b, 0x0d, 0xa7, 0x6a, 0xa3, 0x52,
        0xb5, 0x2a, 0x01, 0xf5, 0x8a, 0x69, 0xe7, 0xf7, 0xd9, 0x0e, 0xa3, 0xb6, 0x3c, 0x38, 0x6d, 0x55, 0xd0, 0xb2, 0x5e, 0x76,
    },
};

// Bumped by every block written, so that furi_hal_otp_valid() looks again
static uint32_t furi_hal_otp_generation;

static const FuriHalOtpBlockLayout* furi_hal_otp_block_layout(FuriHalOtpBlock block) {
    furi_check(block >= FuriHalOtpBlock1 && block <= FuriHalOtpBlockCount);
    return &furi_hal_otp_blocks[block];
}

size_t furi_hal_otp_block_size(FuriHalOtpBlock block) {
    return block >= FuriHalOtpBlock1 && block <= FuriHalOtpBlockCount ? furi_hal_otp_blocks[block].size : 0;
}

// CRC-16/CCITT-FALSE of a block but its magic and the CRC itself. ECC reads
// do not report a row they cannot correct; this does.
static uint16_t furi_hal_otp_block_crc(const uint8_t* data, size_t size) {
    uint16_t crc = 0xFFFF;
    for(size_t i = sizeof(uint32_t); i < size - sizeof(uint16_t); i++) {
        crc ^= (uint16_t)data[i] << 8;
        for(int bit = 0; bit < 8; bit++) {
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
        }
    }
    return crc;
}

// Magic, index, version and CRC of a block's bytes
static bool furi_hal_otp_block_check(FuriHalOtpBlock block, const uint8_t* data) {
    const FuriHalOtpBlockLayout* layout = furi_hal_otp_block_layout(block);
    FuriHalOtpHeader header;
    uint16_t crc;
    memcpy(&header, data, sizeof(header));
    memcpy(&crc, data + layout->size - sizeof(crc), sizeof(crc));
    return header.magic == FURI_HAL_OTP_MAGIC && header.index == block && header.version == layout->version &&
           crc == furi_hal_otp_block_crc(data, layout->size);
}

static bool furi_hal_otp_block_read_rows(FuriHalOtpBlock block, void* data) {
    const FuriHalOtpBlockLayout* layout = furi_hal_otp_block_layout(block);
    otp_cmd_t cmd = {.flags = layout->base_row | OTP_CMD_ECC_BITS};
    int rc = furi_hal_otp_access(data, layout->size, cmd);
    if(rc != BOOTROM_OK) {
        FURI_LOG_E(TAG, "OTP block %u read failed: %d", block, rc);
        return false;
    }
    return true;
}

bool furi_hal_otp_block_dump(FuriHalOtpBlock block, void* data, size_t size) {
    furi_check(size == furi_hal_otp_block_layout(block)->size);
    return furi_hal_otp_block_read_rows(block, data);
}

bool furi_hal_otp_block_read(FuriHalOtpBlock block, void* data, size_t size) {
    const FuriHalOtpBlockLayout* layout = furi_hal_otp_block_layout(block);
    furi_check(size == layout->size);
    memset(data, 0, size);
    // The magic is written last, after the rest reads back right: only then
    // are the rows read through ECC
    const uint16_t magic[2] = {FURI_HAL_OTP_MAGIC & 0xFFFF, FURI_HAL_OTP_MAGIC >> 16};
    if(!furi_hal_otp_rows_done(layout->base_row, magic, 2)) return false;
    if(!furi_hal_otp_block_read_rows(block, data) || !furi_hal_otp_block_check(block, data)) {
        FURI_LOG_E(TAG, "OTP block %u is damaged", block);
        memset(data, 0, size);
        return false;
    }
    return true;
}

static bool furi_hal_otp_block_valid(FuriHalOtpBlock block) {
    uint8_t data[FURI_HAL_OTP_BLOCK_MAX_SIZE];
    return furi_hal_otp_block_read(block, data, furi_hal_otp_block_layout(block)->size);
}

// Make the rows hold the block and lock its pages. The rest is written and
// read back first, the magic (rows 0..1) last. A write cut short, even inside
// a row, is finished by the same call with the same block; a row holding
// other data refuses the call before any write.
static FuriHalOtpError furi_hal_otp_block_write(uint16_t base_row, const uint16_t* want, size_t count) {
    FuriHalOtpRows rows = {.done = true};
    if(!furi_hal_otp_rows_scan(&rows, base_row, want, count)) return FuriHalOtpErrorWriteFailed;

    if(!rows.done) {
        if(furi_hal_otp_rows_done(base_row, want, 2)) return FuriHalOtpErrorAlreadyWritten;
        if(rows.other) return FuriHalOtpErrorNotBlank;

        for(size_t i = 2; i < count; i++) {
            int rc = furi_hal_otp_write_ecc_rc(base_row + i, want[i]);
            if(rc != BOOTROM_OK) {
                FURI_LOG_E(TAG, "OTP ECC write row 0x%04x failed: %d", base_row + i, rc);
                return FuriHalOtpErrorWriteFailed;
            }
        }
        // The bootrom does not check what it wrote: the magic only on a block
        // that reads back right, so a bad row can still be written again
        if(!furi_hal_otp_rows_done(base_row + 2, want + 2, count - 2) || !furi_hal_otp_write_ecc(base_row + 1, want[1]) ||
           !furi_hal_otp_write_ecc(base_row, want[0]) || !furi_hal_otp_rows_done(base_row, want, 2)) {
            FURI_LOG_E(TAG, "OTP block at 0x%04x reads back wrong", base_row);
            return FuriHalOtpErrorWriteFailed;
        }
    }

    if(!furi_hal_otp_lock_rows(base_row, count)) return FuriHalOtpErrorLockFailed;
    return FuriHalOtpErrorNone;
}

// Signatures

static bool furi_hal_otp_signature_ok(
    const uint8_t key[FURI_HAL_OTP_SIGN_SIZE],
    const uint8_t chip_id[FURI_HAL_OTP_CHIP_ID_SIZE],
    FuriHalOtpBlock block,
    const void* data,
    size_t size,
    const uint8_t signature[FURI_HAL_OTP_SIGN_SIZE]) {
    // chip id || "-otp<n>-" || block
    char label[8];
    const int label_len = snprintf(label, sizeof(label), "-otp%u-", block);
    pico_sha256_state_t state;
    sha256_result_t hash;
    if(pico_sha256_start_blocking(&state, SHA256_BIG_ENDIAN, false) != PICO_OK) return false;
    pico_sha256_update_blocking(&state, chip_id, FURI_HAL_OTP_CHIP_ID_SIZE);
    pico_sha256_update_blocking(&state, (const uint8_t*)label, label_len);
    pico_sha256_update_blocking(&state, data, size);
    pico_sha256_finish(&state, &hash);
    return uECC_verify(key, hash.bytes, sizeof(hash.bytes), signature, uECC_secp256r1());
}

static bool furi_hal_otp_signatures_ok(const FuriHalOtpBlock1Data* b1, const FuriHalOtpBlock2Data* b2, const FuriHalOtpBlock3Data* b3) {
    if(b3->key_id >= COUNT_OF(furi_hal_otp_keys)) {
        FURI_LOG_E(TAG, "OTP block 3: unknown key id %u", b3->key_id);
        return false;
    }
    const uint8_t* key = furi_hal_otp_keys[b3->key_id];
    return furi_hal_otp_signature_ok(key, b1->chip_id, FuriHalOtpBlock1, b1, sizeof(*b1), b3->block1_signature) &&
           furi_hal_otp_signature_ok(key, b1->chip_id, FuriHalOtpBlock2, b2, sizeof(*b2), b3->block2_signature);
}

void furi_hal_otp_chip_id(uint8_t chip_id[FURI_HAL_OTP_CHIP_ID_SIZE]) {
    pico_unique_board_id_t id;
    pico_get_unique_board_id(&id);
    memcpy(chip_id, id.id, FURI_HAL_OTP_CHIP_ID_SIZE);
}

bool furi_hal_otp_valid(void) {
    static uint32_t checked_generation = UINT32_MAX;
    static bool valid = false;

    furi_hal_otp_acquire();
    if(checked_generation != furi_hal_otp_generation) {
        FuriHalOtpBlock1Data b1;
        FuriHalOtpBlock2Data b2;
        FuriHalOtpBlock3Data b3;
        valid = furi_hal_otp_block_read(FuriHalOtpBlock1, &b1, sizeof(b1)) && furi_hal_otp_block_read(FuriHalOtpBlock2, &b2, sizeof(b2)) &&
                furi_hal_otp_block_read(FuriHalOtpBlock3, &b3, sizeof(b3)) && furi_hal_otp_signatures_ok(&b1, &b2, &b3);
        checked_generation = furi_hal_otp_generation;
    }
    const bool result = valid;
    furi_hal_otp_release();
    return result;
}

// Writing a block

static FuriHalOtpError furi_hal_otp_block_program_locked(FuriHalOtpBlock block, const void* data, size_t size) {
    const FuriHalOtpBlockLayout* layout = furi_hal_otp_block_layout(block);
    uint16_t want[FURI_HAL_OTP_BLOCK_MAX_SIZE / 2];
    if(size != layout->size) return FuriHalOtpErrorInvalidBlock;
    memcpy(want, data, size);
    if(!furi_hal_otp_block_check(block, (const uint8_t*)want)) return FuriHalOtpErrorInvalidBlock;

    for(FuriHalOtpBlock before = FuriHalOtpBlock1; before < block; before++) {
        if(!furi_hal_otp_block_valid(before)) return FuriHalOtpErrorOrder;
    }

    char board_id[34];
    if(block == FuriHalOtpBlock1) {
        FuriHalOtpBlock1Data b1;
        uint8_t chip_id[FURI_HAL_OTP_CHIP_ID_SIZE];
        memcpy(&b1, want, sizeof(b1));
        furi_hal_otp_chip_id(chip_id);
        if(memcmp(b1.chip_id, chip_id, sizeof(chip_id))) return FuriHalOtpErrorChipId;
        // The white label carries the same board id: one with another,
        // finished or cut short, is refused before anything is written
        if(furi_hal_otp_usb_white_label_board_id_differs(b1.target, b1.body, b1.connect)) return FuriHalOtpErrorWhiteLabelMismatch;
        snprintf(board_id, sizeof(board_id), OTP_BOARD_ID_FORMAT, (uint32_t)b1.target, (uint32_t)b1.body, (uint32_t)b1.connect);
    } else if(block == FuriHalOtpBlock3) {
        // Locked for good: only signatures that verify
        FuriHalOtpBlock1Data b1;
        FuriHalOtpBlock2Data b2;
        FuriHalOtpBlock3Data b3;
        memcpy(&b3, want, sizeof(b3));
        if(!furi_hal_otp_block_read(FuriHalOtpBlock1, &b1, sizeof(b1)) || !furi_hal_otp_block_read(FuriHalOtpBlock2, &b2, sizeof(b2)) ||
           !furi_hal_otp_signatures_ok(&b1, &b2, &b3)) {
            return FuriHalOtpErrorSignature;
        }
    }

    FuriHalOtpError error = furi_hal_otp_block_write(layout->base_row, want, size / 2);
    furi_hal_otp_generation++;
    if(error != FuriHalOtpErrorNone || block != FuriHalOtpBlock1) return error;

    // A white label written before the locks existed is locked now
    if(furi_hal_otp_usb_white_label_reads_back(board_id)) {
        return furi_hal_otp_usb_white_label_lock() ? FuriHalOtpErrorNone : FuriHalOtpErrorWhiteLabelFailed;
    }
    FuriHalOtpBlock1Data b1;
    memcpy(&b1, want, sizeof(b1));
    FuriHalOtpUsbWhiteLabelError wl_error = furi_hal_otp_write_usb_white_label(b1.target, b1.body, b1.connect);
    if(wl_error != FuriHalOtpUsbWhiteLabelErrorNone) {
        FURI_LOG_E(TAG, "USB white label: %s", furi_hal_otp_usb_white_label_error_to_string(wl_error));
        return FuriHalOtpErrorWhiteLabelFailed;
    }
    return FuriHalOtpErrorNone;
}

FuriHalOtpError furi_hal_otp_block_program(FuriHalOtpBlock block, const void* data, size_t size) {
    if(block < FuriHalOtpBlock1 || block > FuriHalOtpBlockCount) return FuriHalOtpErrorInvalidBlock;
    furi_hal_otp_acquire();
    FuriHalOtpError error = furi_hal_otp_block_program_locked(block, data, size);
    furi_hal_otp_release();
    return error;
}

const char* furi_hal_otp_error_to_string(FuriHalOtpError error) {
    switch(error) {
    case FuriHalOtpErrorNone:
        return "None";
    case FuriHalOtpErrorInvalidBlock:
        return "Not a valid block: size, magic, index, version or CRC";
    case FuriHalOtpErrorOrder:
        return "An earlier block is not written";
    case FuriHalOtpErrorChipId:
        return "Block 1 is made for another chip";
    case FuriHalOtpErrorSignature:
        return "Unknown key, or a signature that does not verify";
    case FuriHalOtpErrorAlreadyWritten:
        return "Already written with other data";
    case FuriHalOtpErrorNotBlank:
        return "Holds other data from a write cut short";
    case FuriHalOtpErrorWriteFailed:
        return "Write failed";
    case FuriHalOtpErrorLockFailed:
        return "Written, but locking its pages failed";
    case FuriHalOtpErrorWhiteLabelFailed:
        return "Block 1 written, USB white label failed";
    case FuriHalOtpErrorWhiteLabelMismatch:
        return "The USB white label holds another board id";
    }
    return "Unknown error";
}
