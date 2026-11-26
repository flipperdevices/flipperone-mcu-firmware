#include <furi.h>
#include <furi_hal_otp.h>
#include <furi_hal_otp_keys.h>
#include <pico/bootrom.h>
#include <hardware/structs/otp.h>
#include <hardware/regs/otp_data.h>
#include <string.h>
#include <pico/unique_id.h>
#include <pico/sha256.h>
#include <uECC.h>

#define TAG "FuriHalOtp"

// Every bootrom OTP call goes through it: the bootrom takes its OTP lock
// without waiting. Held across a whole block write too, so two writers cannot
// both find it blank.
static FuriMutex* furi_hal_otp_mutex;

// clang-format off

// USB white label: a 16 row table at WL_BASE_ROW, its strings right after it
#define WL_BASE_ROW 0x0400

// String in the table: row offset from WL_BASE_ROW, char count
#define WL_STRDEF(row_offset, char_count) (((row_offset) << 8) | (char_count))

// "" makes sure the argument is a string literal
#define STR_LEN(str)  (sizeof("" str) - 1)
#define STR_ROWS(str) (sizeof("" str) / 2)

#define OTP_USB_VID          0x37C1
#define OTP_USB_PID          0xF102
#define OTP_USB_BCD          0x0100
#define OTP_USB_LANG_ID      0x0409
#define OTP_USB_MAX_POWER    0xFA
#define OTP_USB_ATTRIBUTES   0x80
#define OTP_USB_MANUFACTURER "Flipper FZCO"
#define OTP_USB_PRODUCT      "Flipper One MCU Bootloader"
#define OTP_USB_VOLUME_LABEL "FlipOneMCU"
#define OTP_SCSI_VENDOR      "Flipper"
#define OTP_SCSI_PRODUCT     "One"
#define OTP_SCSI_VERSION     "1.00"
#define OTP_REDIRECT_URL     "https://r.flipper.net/flipper_one_mcu_update"
#define OTP_REDIRECT_NAME    "How to Update Firmware"
#define OTP_MODEL            "Flipper One"
#define OTP_BOARD_ID_FORMAT  "F%luB%luC%lu"
// The longest board id: 10 digits each
#define OTP_BOARD_ID_SIZE    34

#define OTP_BOOTSEL_LED_PIN  35

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
#define WL_ROWS              (WL_OFF_BOARD_ID + OTP_BOARD_ID_SIZE / 2)
// Longest string the table can point to: 127 chars
#define WL_STRING_MAX_ROWS   64

static_assert(STR_LEN(OTP_USB_MANUFACTURER) <= 30,  "OTP_USB_MANUFACTURER too long");
static_assert(STR_LEN(OTP_USB_PRODUCT)      <= 30,  "OTP_USB_PRODUCT too long");
static_assert(STR_LEN(OTP_USB_VOLUME_LABEL) <= 11,  "OTP_USB_VOLUME_LABEL too long");
static_assert(STR_LEN(OTP_SCSI_VENDOR)      <= 8,   "OTP_SCSI_VENDOR too long");
static_assert(STR_LEN(OTP_SCSI_PRODUCT)     <= 16,  "OTP_SCSI_PRODUCT too long");
static_assert(STR_LEN(OTP_SCSI_VERSION)     <= 4,   "OTP_SCSI_VERSION too long");
static_assert(STR_LEN(OTP_REDIRECT_URL)     <= 127, "OTP_REDIRECT_URL too long");
static_assert(STR_LEN(OTP_REDIRECT_NAME)    <= 127, "OTP_REDIRECT_NAME too long");
static_assert(STR_LEN(OTP_MODEL)            <= 127, "OTP_MODEL too long");
static_assert(WL_BASE_ROW % NUM_OTP_PAGE_ROWS + WL_ROWS <= 2 * NUM_OTP_PAGE_ROWS, "USB white label outgrows pages 16..17");

// USB_BOOT_FLAGS of the white label: every entry written is valid. There is
// no serial number string (SERIAL_NUMBER_STRDEF); entry 2 is bcdDevice.
#define WL_USB_BOOT_FLAGS ( \
    OTP_DATA_USB_BOOT_FLAGS_WHITE_LABEL_ADDR_VALID_BITS | \
    OTP_DATA_USB_BOOT_FLAGS_WL_USB_DEVICE_VID_VALUE_VALID_BITS | \
    OTP_DATA_USB_BOOT_FLAGS_WL_USB_DEVICE_PID_VALUE_VALID_BITS | \
    OTP_DATA_USB_BOOT_FLAGS_WL_USB_DEVICE_SERIAL_NUMBER_VALUE_VALID_BITS | \
    OTP_DATA_USB_BOOT_FLAGS_WL_USB_DEVICE_LANG_ID_VALUE_VALID_BITS | \
    OTP_DATA_USB_BOOT_FLAGS_WL_USB_DEVICE_MANUFACTURER_STRDEF_VALID_BITS | \
    OTP_DATA_USB_BOOT_FLAGS_WL_USB_DEVICE_PRODUCT_STRDEF_VALID_BITS | \
    OTP_DATA_USB_BOOT_FLAGS_WL_USB_CONFIG_ATTRIBUTES_MAX_POWER_VALUES_VALID_BITS | \
    OTP_DATA_USB_BOOT_FLAGS_WL_VOLUME_LABEL_STRDEF_VALID_BITS | \
    OTP_DATA_USB_BOOT_FLAGS_WL_SCSI_INQUIRY_VENDOR_STRDEF_VALID_BITS | \
    OTP_DATA_USB_BOOT_FLAGS_WL_SCSI_INQUIRY_PRODUCT_STRDEF_VALID_BITS | \
    OTP_DATA_USB_BOOT_FLAGS_WL_SCSI_INQUIRY_VERSION_STRDEF_VALID_BITS | \
    OTP_DATA_USB_BOOT_FLAGS_WL_INDEX_HTM_REDIRECT_URL_STRDEF_VALID_BITS | \
    OTP_DATA_USB_BOOT_FLAGS_WL_INDEX_HTM_REDIRECT_NAME_STRDEF_VALID_BITS | \
    OTP_DATA_USB_BOOT_FLAGS_WL_INFO_UF2_TXT_MODEL_STRDEF_VALID_BITS | \
    OTP_DATA_USB_BOOT_FLAGS_WL_INFO_UF2_TXT_BOARD_ID_STRDEF_VALID_BITS)

// PAGEn_LOCK1 in each of its three copies: read only for Secure, Non-secure
// and bootloader access
#define OTP_PAGE_LOCK1_NS_BL \
    ((OTP_DATA_PAGE0_LOCK1_LOCK_NS_VALUE_READ_ONLY << OTP_DATA_PAGE0_LOCK1_LOCK_NS_LSB) | \
     (OTP_DATA_PAGE0_LOCK1_LOCK_BL_VALUE_READ_ONLY << OTP_DATA_PAGE0_LOCK1_LOCK_BL_LSB))
#define OTP_PAGE_LOCK1_READ_ONLY \
    ((OTP_DATA_PAGE0_LOCK1_LOCK_S_VALUE_READ_ONLY << OTP_DATA_PAGE0_LOCK1_LOCK_S_LSB) | OTP_PAGE_LOCK1_NS_BL)
#define OTP_COPIES(value) ((value) * 0x010101u)

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

// Rows

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

// Each bit as the hardware takes it from a raw row's three copies
static bool furi_hal_otp_read_vote(uint16_t row, uint32_t* value) {
    uint32_t a = 0, b = 0, c = 0;
    if(!furi_hal_otp_read_raw(row, &a) || !furi_hal_otp_read_raw(row + 1, &b) || !furi_hal_otp_read_raw(row + 2, &c)) return false;
    *value = (a & b) | (a & c) | (b & c);
    return true;
}

// An ECC row as the bootrom writes it: 16 data bits and 6 parity bits (from
// pico-bootrom-rp2350 src/main/arm/varm_otp.c, s_otp_calculate_ecc,
// BSD-3-Clause). Where the set bits do not fit, it writes the row inverted
// with bits 23:22 set instead (bit repair by polarity).
static uint32_t furi_hal_otp_ecc(uint16_t value) {
    static const uint32_t parity[6] = {0x0AD5B, 0x0366D, 0x0C78E, 0x007F0, 0x0F800, 0x1FFFFF};
    uint32_t row = value;
    for(size_t i = 0; i < COUNT_OF(parity); i++) {
        row |= (uint32_t)__builtin_parity(row & parity[i]) << (16 + i);
    }
    return row;
}

#define OTP_ECC_BRP(row) (0xC00000u | ((row) ^ 0x3FFFFFu))

// Whether raw bits are a whole ECC row of some value, as is or inverted
static bool furi_hal_otp_ecc_whole(uint32_t raw) {
    if((raw & 0xC00000u) == 0xC00000u) raw = (raw ^ 0x3FFFFFu) & 0x3FFFFFu;
    return !(raw & 0xC00000u) && furi_hal_otp_ecc(raw & 0xFFFF) == raw;
}

typedef enum {
    FuriHalOtpRowDone, // holds the wanted value
    FuriHalOtpRowToWrite, // blank, or a write of the wanted value cut short
    FuriHalOtpRowOther, // holds another value, or bits the wanted one has not
} FuriHalOtpRowState;

// Only raw bits tell a row cut short from one holding other data. A whole row
// of another value is other data even where its bits fit in the wanted
// value's: a write cut short that ends as one is taken as other data too.
static bool furi_hal_otp_row_state(uint16_t row, uint16_t want, FuriHalOtpRowState* state) {
    uint32_t raw = 0;
    if(!furi_hal_otp_read_raw(row, &raw)) return false;
    const uint32_t ecc = furi_hal_otp_ecc(want);
    if(raw == ecc || raw == OTP_ECC_BRP(ecc)) {
        *state = FuriHalOtpRowDone;
    } else if(raw && furi_hal_otp_ecc_whole(raw)) {
        *state = FuriHalOtpRowOther;
    } else if(!(raw & ~ecc)) {
        *state = FuriHalOtpRowToWrite;
    } else {
        *state = FuriHalOtpRowOther;
    }
    return true;
}

// Whether any of count rows from first_row holds other data
static bool furi_hal_otp_rows_other(uint16_t first_row, const uint16_t* want, size_t count, bool* other) {
    for(size_t i = 0; i < count; i++) {
        FuriHalOtpRowState state;
        if(!furi_hal_otp_row_state(first_row + i, want[i], &state)) return false;
        if(state == FuriHalOtpRowOther) *other = true;
    }
    return true;
}

// Whether count rows from first_row read as their wanted values through ECC,
// which corrects a bit that did not program
static bool furi_hal_otp_rows_done(uint16_t first_row, const uint16_t* want, size_t count) {
    for(size_t i = 0; i < count; i++) {
        uint16_t have = 0;
        otp_cmd_t cmd = {.flags = (first_row + i) | OTP_CMD_ECC_BITS};
        if(furi_hal_otp_access((uint8_t*)&have, sizeof(have), cmd) != BOOTROM_OK || have != want[i]) return false;
    }
    return true;
}

static bool furi_hal_otp_write_ecc(uint16_t first_row, const uint16_t* values, size_t count) {
    otp_cmd_t cmd = {.flags = first_row | OTP_CMD_ECC_BITS | OTP_CMD_WRITE_BITS};
    int rc = furi_hal_otp_access((uint8_t*)values, count * sizeof(uint16_t), cmd);
    if(rc != BOOTROM_OK) {
        FURI_LOG_E(TAG, "OTP ECC write row 0x%04x failed: %d", first_row, rc);
        return false;
    }
    return true;
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

static bool furi_hal_otp_write_raw_copies(uint16_t row, uint32_t value) {
    return furi_hal_otp_write_raw(row, value) && furi_hal_otp_write_raw(row + 1, value) && furi_hal_otp_write_raw(row + 2, value);
}

// Page locks

// Permanent from the next reset; SW_LOCKn holds it until then. A page's lock
// row is read only to the access its own lock denies, so Secure goes last:
// cut short before it, this Secure code can still finish the row.
static bool furi_hal_otp_lock_page(uint32_t page) {
    furi_check(page < NUM_OTP_PAGES);
    const uint16_t row = OTP_DATA_PAGE0_LOCK1_ROW + page * (OTP_DATA_PAGE1_LOCK1_ROW - OTP_DATA_PAGE0_LOCK1_ROW);
    const bool written = furi_hal_otp_write_raw(row, OTP_COPIES(OTP_PAGE_LOCK1_NS_BL)) && furi_hal_otp_write_raw(row, OTP_COPIES(OTP_PAGE_LOCK1_READ_ONLY));
    // The bootrom does not check what it wrote: each bit as two of its three copies
    uint32_t lock = 0;
    if(!furi_hal_otp_read_raw(row, &lock)) return false;
    const uint32_t a = lock & 0xFF, b = (lock >> 8) & 0xFF, c = (lock >> 16) & 0xFF;
    if(((a & b) | (a & c) | (b & c)) != OTP_PAGE_LOCK1_READ_ONLY) {
        FURI_LOG_E(TAG, "OTP page %lu: lock row %s", page, written ? "reads back wrong" : "write failed");
        return false;
    }
    otp_hw->sw_lock[page] = (OTP_SW_LOCK0_SEC_VALUE_READ_ONLY << OTP_SW_LOCK0_SEC_LSB) | (OTP_SW_LOCK0_NSEC_VALUE_READ_ONLY << OTP_SW_LOCK0_NSEC_LSB);
    return true;
}

static bool furi_hal_otp_lock_rows(uint32_t first_row, uint32_t row_count) {
    for(uint32_t page = first_row / NUM_OTP_PAGE_ROWS; page <= (first_row + row_count - 1) / NUM_OTP_PAGE_ROWS; page++) {
        if(!furi_hal_otp_lock_page(page)) return false;
    }
    return true;
}

// USB white label

bool furi_hal_otp_usb_white_label_valid(void) {
    // As the bootrom reads it: each flag by two of its three copies
    uint32_t flags = 0;
    return furi_hal_otp_read_vote(OTP_DATA_USB_BOOT_FLAGS_ROW, &flags) && (flags & WL_USB_BOOT_FLAGS) == WL_USB_BOOT_FLAGS;
}

typedef struct {
    uint8_t offset;
    const char* str;
} FuriHalOtpWlString;

static const FuriHalOtpWlString furi_hal_otp_wl_strings[] = {
    {WL_OFF_MANUFACTURER, OTP_USB_MANUFACTURER},
    {WL_OFF_PRODUCT, OTP_USB_PRODUCT},
    {WL_OFF_VOLUME_LABEL, OTP_USB_VOLUME_LABEL},
    {WL_OFF_SCSI_VENDOR, OTP_SCSI_VENDOR},
    {WL_OFF_SCSI_PRODUCT, OTP_SCSI_PRODUCT},
    {WL_OFF_SCSI_VERSION, OTP_SCSI_VERSION},
    {WL_OFF_REDIRECT_URL, OTP_REDIRECT_URL},
    {WL_OFF_REDIRECT_NAME, OTP_REDIRECT_NAME},
    {WL_OFF_MODEL, OTP_MODEL},
};

static void furi_hal_otp_wl_table(uint16_t table[16], size_t board_id_len) {
    // clang-format off
    const uint16_t values[16] = {
        [0]  = OTP_USB_VID,
        [1]  = OTP_USB_PID,
        [2]  = OTP_USB_BCD,
        [3]  = OTP_USB_LANG_ID,
        [4]  = WL_STRDEF(WL_OFF_MANUFACTURER,  STR_LEN(OTP_USB_MANUFACTURER)),
        [5]  = WL_STRDEF(WL_OFF_PRODUCT,       STR_LEN(OTP_USB_PRODUCT)),
        [6]  = 0x0000, // serial number string
        [7]  = (OTP_USB_MAX_POWER << 8) | OTP_USB_ATTRIBUTES,
        [8]  = WL_STRDEF(WL_OFF_VOLUME_LABEL,  STR_LEN(OTP_USB_VOLUME_LABEL)),
        [9]  = WL_STRDEF(WL_OFF_SCSI_VENDOR,   STR_LEN(OTP_SCSI_VENDOR)),
        [10] = WL_STRDEF(WL_OFF_SCSI_PRODUCT,  STR_LEN(OTP_SCSI_PRODUCT)),
        [11] = WL_STRDEF(WL_OFF_SCSI_VERSION,  STR_LEN(OTP_SCSI_VERSION)),
        [12] = WL_STRDEF(WL_OFF_REDIRECT_URL,  STR_LEN(OTP_REDIRECT_URL)),
        [13] = WL_STRDEF(WL_OFF_REDIRECT_NAME, STR_LEN(OTP_REDIRECT_NAME)),
        [14] = WL_STRDEF(WL_OFF_MODEL,         STR_LEN(OTP_MODEL)),
        [15] = WL_STRDEF(WL_OFF_BOARD_ID,      board_id_len),
    };
    // clang-format on
    memcpy(table, values, sizeof(values));
}

// ASCII, 2 chars per row, little endian: the row count
static size_t furi_hal_otp_wl_string_rows(const char* str, uint16_t rows[WL_STRING_MAX_ROWS]) {
    const size_t len = strlen(str);
    memset(rows, 0, WL_STRING_MAX_ROWS * sizeof(uint16_t));
    memcpy(rows, str, len);
    return (len + 1) / 2;
}

typedef enum {
    FuriHalOtpWlScan, // find rows holding other data
    FuriHalOtpWlCheck, // all rows read right
    FuriHalOtpWlWrite,
} FuriHalOtpWlOp;

static bool furi_hal_otp_wl_rows(FuriHalOtpWlOp op, uint16_t row, const uint16_t* want, size_t count, bool* other) {
    switch(op) {
    case FuriHalOtpWlScan:
        return furi_hal_otp_rows_other(row, want, count, other);
    case FuriHalOtpWlCheck:
        return furi_hal_otp_rows_done(row, want, count);
    default:
        return furi_hal_otp_write_ecc(row, want, count);
    }
}

// Every ECC row of the white label with this board id: table, strings, address
static bool furi_hal_otp_wl_each(const char* board_id, FuriHalOtpWlOp op, bool* other) {
    uint16_t rows[WL_STRING_MAX_ROWS];
    furi_hal_otp_wl_table(rows, strlen(board_id));
    if(!furi_hal_otp_wl_rows(op, WL_BASE_ROW, rows, 16, other)) return false;
    for(size_t i = 0; i < COUNT_OF(furi_hal_otp_wl_strings); i++) {
        const size_t count = furi_hal_otp_wl_string_rows(furi_hal_otp_wl_strings[i].str, rows);
        if(!furi_hal_otp_wl_rows(op, WL_BASE_ROW + furi_hal_otp_wl_strings[i].offset, rows, count, other)) return false;
    }
    const size_t count = furi_hal_otp_wl_string_rows(board_id, rows);
    if(!furi_hal_otp_wl_rows(op, WL_BASE_ROW + WL_OFF_BOARD_ID, rows, count, other)) return false;
    const uint16_t addr = WL_BASE_ROW;
    return furi_hal_otp_wl_rows(op, OTP_DATA_USB_WHITE_LABEL_ADDR_ROW, &addr, 1, other);
}

static bool furi_hal_otp_wl_reads_back(const char* board_id) {
    return furi_hal_otp_wl_each(board_id, FuriHalOtpWlCheck, NULL) && furi_hal_otp_usb_white_label_valid();
}

// Whether the white label, finished or cut short, holds data that is not of
// this board id. False if OTP cannot be read.
static bool furi_hal_otp_wl_differs(const char* board_id, bool* differs) {
    bool other = false;
    if(!furi_hal_otp_wl_each(board_id, FuriHalOtpWlScan, &other)) return false;
    *differs = other || (furi_hal_otp_usb_white_label_valid() && !furi_hal_otp_wl_reads_back(board_id));
    return true;
}

static bool furi_hal_otp_wl_lock(void) {
    return furi_hal_otp_lock_rows(WL_BASE_ROW, WL_ROWS);
}

// Written and locked; one already written is only locked
static bool furi_hal_otp_wl_write(const char* board_id) {
    if(!furi_hal_otp_wl_reads_back(board_id)) {
        if(!furi_hal_otp_wl_each(board_id, FuriHalOtpWlWrite, NULL) || !furi_hal_otp_write_raw_copies(OTP_DATA_USB_BOOT_FLAGS_ROW, WL_USB_BOOT_FLAGS)) {
            return false;
        }
        // The bootrom does not check what it wrote
        if(!furi_hal_otp_wl_reads_back(board_id)) {
            FURI_LOG_E(TAG, "USB white label reads back wrong");
            return false;
        }
    }
    return furi_hal_otp_wl_lock();
}

// BOOTSEL LED

bool furi_hal_otp_bootsel_led_valid(void) {
    uint32_t flags = 0;
    return furi_hal_otp_read_raw(OTP_DATA_BOOT_FLAGS0_ROW, &flags) && (flags & OTP_DATA_BOOT_FLAGS0_ENABLE_BOOTSEL_LED_BITS);
}

bool furi_hal_otp_write_bootsel_led(void) {
    // Active high: ACTIVELOW is broken (https://forums.raspberrypi.com/viewtopic.php?t=397828)
    const uint16_t led_cfg = (OTP_BOOTSEL_LED_PIN << OTP_DATA_BOOTSEL_LED_CFG_PIN_LSB) & OTP_DATA_BOOTSEL_LED_CFG_PIN_BITS;
    return furi_hal_otp_write_ecc(OTP_DATA_BOOTSEL_LED_CFG_ROW, &led_cfg, 1) &&
           furi_hal_otp_write_raw_copies(OTP_DATA_BOOT_FLAGS0_ROW, OTP_DATA_BOOT_FLAGS0_ENABLE_BOOTSEL_LED_BITS);
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
static_assert(sizeof(FuriHalOtpBlock3Data) == 214, "OTP block 3 size");
static_assert(0x0C0 + sizeof(FuriHalOtpBlock1Data) / 2 <= 0x100, "OTP block 1 outgrows page 3");
static_assert(0x100 + sizeof(FuriHalOtpBlock2Data) / 2 <= 0x180, "OTP block 2 outgrows pages 4..5");
static_assert(0x180 + sizeof(FuriHalOtpBlock3Data) / 2 <= 0x200, "OTP block 3 outgrows pages 6..7");
static_assert(sizeof(FuriHalOtpBlock1Data) <= FURI_HAL_OTP_BLOCK_MAX_SIZE && sizeof(FuriHalOtpBlock2Data) <= FURI_HAL_OTP_BLOCK_MAX_SIZE, "OTP block max size");
static_assert(PICO_UNIQUE_BOARD_ID_SIZE_BYTES == FURI_HAL_OTP_CHIP_ID_SIZE, "chip id size");
static_assert(FURI_HAL_OTP_KEY_SIZE == FURI_HAL_OTP_PUBLIC_SIZE, "public key size");

// Bumped by every block written, so that furi_hal_otp_valid() looks again
static uint32_t furi_hal_otp_generation;

static const FuriHalOtpBlockLayout* furi_hal_otp_block_layout(FuriHalOtpBlock block) {
    furi_check(block >= FuriHalOtpBlock1 && block <= FuriHalOtpBlockCount);
    return &furi_hal_otp_blocks[block];
}

size_t furi_hal_otp_block_size(FuriHalOtpBlock block) {
    return block >= FuriHalOtpBlock1 && block <= FuriHalOtpBlockCount ? furi_hal_otp_blocks[block].size : 0;
}

// CRC-16/CCITT-FALSE of all but the magic and the CRC itself. ECC reads do
// not report a row they cannot correct; this does.
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
    // ECC reads take a 2-byte aligned buffer; data may be a packed struct
    uint16_t rows[FURI_HAL_OTP_BLOCK_MAX_SIZE / 2];
    otp_cmd_t cmd = {.flags = layout->base_row | OTP_CMD_ECC_BITS};
    int rc = furi_hal_otp_access((uint8_t*)rows, layout->size, cmd);
    if(rc != BOOTROM_OK) {
        FURI_LOG_E(TAG, "OTP block %u read failed: %d", block, rc);
        return false;
    }
    memcpy(data, rows, layout->size);
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
    // The magic is written last: a block without it is not written
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

// Rows 2.. first, read back, then the magic (rows 0..1), then the locks. The
// same call with the same block finishes one cut short, even inside a row.
static FuriHalOtpError furi_hal_otp_block_write(uint16_t base_row, const uint16_t* want, size_t count) {
    bool other = false;
    if(!furi_hal_otp_rows_other(base_row, want, count, &other)) return FuriHalOtpErrorWriteFailed;

    if(!furi_hal_otp_rows_done(base_row, want, count)) {
        if(furi_hal_otp_rows_done(base_row, want, 2)) return FuriHalOtpErrorAlreadyWritten;
        if(other) return FuriHalOtpErrorNotBlank;

        if(!furi_hal_otp_write_ecc(base_row + 2, want + 2, count - 2)) return FuriHalOtpErrorWriteFailed;
        // The bootrom does not check what it wrote: no magic on a block that
        // reads back wrong, so a bad row can still be written again
        if(!furi_hal_otp_rows_done(base_row + 2, want + 2, count - 2) || !furi_hal_otp_write_ecc(base_row + 1, &want[1], 1) ||
           !furi_hal_otp_write_ecc(base_row, &want[0], 1) || !furi_hal_otp_rows_done(base_row, want, 2)) {
            FURI_LOG_E(TAG, "OTP block at 0x%04x reads back wrong", base_row);
            return FuriHalOtpErrorWriteFailed;
        }
    } else {
        // A row a bit short reads right through ECC: finish it while the page
        // is open, so the correction is not spent
        for(size_t i = 0; i < count; i++) {
            FuriHalOtpRowState state;
            if(furi_hal_otp_row_state(base_row + i, want[i], &state) && state == FuriHalOtpRowToWrite) {
                furi_hal_otp_write_ecc(base_row + i, &want[i], 1);
            }
        }
    }

    if(!furi_hal_otp_lock_rows(base_row, count)) return FuriHalOtpErrorLockFailed;
    return FuriHalOtpErrorNone;
}

// Signatures

static void furi_hal_otp_chip_id(uint8_t chip_id[FURI_HAL_OTP_CHIP_ID_SIZE]) {
    pico_unique_board_id_t id;
    pico_get_unique_board_id(&id);
    memcpy(chip_id, id.id, FURI_HAL_OTP_CHIP_ID_SIZE);
}

// Over chip id || "-otp<n>-" || block
static bool furi_hal_otp_signature_ok(
    const uint8_t key[FURI_HAL_OTP_PUBLIC_SIZE],
    const uint8_t chip_id[FURI_HAL_OTP_CHIP_ID_SIZE],
    FuriHalOtpBlock block,
    const void* data,
    size_t size,
    const uint8_t signature[FURI_HAL_OTP_SIGN_SIZE]) {
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
    // Only this chip's id makes them this device's, not a copy of another's OTP
    uint8_t chip_id[FURI_HAL_OTP_CHIP_ID_SIZE];
    furi_hal_otp_chip_id(chip_id);
    if(memcmp(b1->chip_id, chip_id, sizeof(chip_id))) {
        FURI_LOG_E(TAG, "OTP block 1: made for another chip");
        return false;
    }
    // The key the block carries is only trusted as one the firmware carries
    if(b3->key_id >= COUNT_OF(furi_hal_otp_keys) || b3->curve != FuriHalOtpCurveSecp256r1 ||
       memcmp(b3->public_key, furi_hal_otp_keys[b3->key_id], FURI_HAL_OTP_PUBLIC_SIZE)) {
        FURI_LOG_E(TAG, "OTP block 3: unknown key %u or curve %u", b3->key_id, b3->curve);
        return false;
    }
    return furi_hal_otp_signature_ok(b3->public_key, b1->chip_id, FuriHalOtpBlock1, b1, sizeof(*b1), b3->block1_signature) &&
           furi_hal_otp_signature_ok(b3->public_key, b1->chip_id, FuriHalOtpBlock2, b2, sizeof(*b2), b3->block2_signature);
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

    char board_id[OTP_BOARD_ID_SIZE];
    if(block == FuriHalOtpBlock1) {
        FuriHalOtpBlock1Data b1;
        uint8_t chip_id[FURI_HAL_OTP_CHIP_ID_SIZE];
        memcpy(&b1, want, sizeof(b1));
        furi_hal_otp_chip_id(chip_id);
        if(memcmp(b1.chip_id, chip_id, sizeof(chip_id))) return FuriHalOtpErrorChipId;
        snprintf(board_id, sizeof(board_id), OTP_BOARD_ID_FORMAT, (uint32_t)b1.target, (uint32_t)b1.body, (uint32_t)b1.connect);
        // The white label is written after the block, which is then locked:
        // one that cannot take this board id is refused before anything
        bool differs = false;
        if(!furi_hal_otp_wl_differs(board_id, &differs)) return FuriHalOtpErrorWriteFailed;
        if(differs) return FuriHalOtpErrorWhiteLabelMismatch;
    } else if(block == FuriHalOtpBlock3) {
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
    // Block 1 goes on to the white label even if its lock failed, so a lock
    // row that cannot be finished does not keep the white label out for good
    if(block != FuriHalOtpBlock1 || (error != FuriHalOtpErrorNone && error != FuriHalOtpErrorLockFailed)) return error;
    if(!furi_hal_otp_wl_write(board_id)) return FuriHalOtpErrorWhiteLabelFailed;
    return error;
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
        return "Unknown key or curve, or a signature that does not verify";
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
        return "The USB white label holds other data";
    }
    return "Unknown error";
}
