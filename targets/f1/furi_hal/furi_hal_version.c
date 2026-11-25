#include <furi_hal_version.h>
#include <furi_hal_otp.h>

#include <furi.h>
#include <pico/unique_id.h>

#define TAG "FuriHalVersion"

#define FLIPPER_MAC_0 0x0C
#define FLIPPER_MAC_1 0xFA
#define FLIPPER_MAC_2 0x22

static uint8_t ble_mac[6] = {FLIPPER_MAC_0, FLIPPER_MAC_1, FLIPPER_MAC_2, 0, 0, 0};

const struct Version* furi_hal_version_get_firmware_version(void) {
    return version_get();
}

const uint8_t* furi_hal_version_get_ble_mac(void) {
    uint32_t uid[3] = {0};
    const uint8_t* uid_ptr = furi_hal_version_uid();
    uid[0] = uid_ptr[0];
    uid[1] = uid_ptr[1];
    uid[2] = uid_ptr[2];

    // Generate a unique MAC address based on the UID
    ble_mac[3] = (uid[0] >> 16) & 0xFF;
    ble_mac[4] = uid[0] & 0xFF;
    ble_mac[5] = (uid[1] >> 16) & 0xFF;

    return ble_mac;
}

// OTP blocks 1 and 2. Written once, so a copy stays true once it is there;
// until then every call looks again, so a block written while running shows
// at once.
static FuriHalOtpBlock1Data furi_hal_version_board;
static volatile bool furi_hal_version_board_valid;
static FuriHalOtpBlock2Data furi_hal_version_device;
static volatile bool furi_hal_version_device_valid;

static const FuriHalOtpBlock1Data* furi_hal_version_get_board(void) {
    if(!furi_hal_version_board_valid) {
        FuriHalOtpBlock1Data board;
        if(furi_hal_otp_block_read(FuriHalOtpBlock1, &board, sizeof(board))) {
            // Published whole: readers see the flag only after the copy
            furi_hal_version_board = board;
            __DMB();
            furi_hal_version_board_valid = true;
        }
    }
    return furi_hal_version_board_valid ? &furi_hal_version_board : NULL;
}

static const FuriHalOtpBlock2Data* furi_hal_version_get_device(void) {
    if(!furi_hal_version_device_valid) {
        FuriHalOtpBlock2Data device;
        if(furi_hal_otp_block_read(FuriHalOtpBlock2, &device, sizeof(device))) {
            furi_hal_version_device = device;
            __DMB();
            furi_hal_version_device_valid = true;
        }
    }
    return furi_hal_version_device_valid ? &furi_hal_version_device : NULL;
}

void furi_hal_version_init(void) {
    const FuriHalOtpBlock1Data* board = furi_hal_version_get_board();
    if(board) {
        FURI_LOG_I(TAG, "Board: version %u, F%uB%uC%u", board->hw_version, board->target, board->body, board->connect);
        // Two ECDSA checks, about 110 ms; cached for furi_hal_otp_valid() callers
        FURI_LOG_I(TAG, "OTP signatures: %s", furi_hal_otp_valid() ? "valid" : "not valid");
    } else {
        FURI_LOG_W(TAG, "No board info in OTP");
    }
}

FuriHalVersionOtpVersion furi_hal_version_get_otp_version(void) {
    const FuriHalOtpBlock1Data* board = furi_hal_version_get_board();
    return board ? (FuriHalVersionOtpVersion)board->header.version : FuriHalVersionOtpVersionEmpty;
}

uint16_t furi_hal_version_get_hw_version(void) {
    const FuriHalOtpBlock1Data* board = furi_hal_version_get_board();
    return board ? board->hw_version : 0;
}

uint16_t furi_hal_version_get_hw_target(void) {
    const FuriHalOtpBlock1Data* board = furi_hal_version_get_board();
    return board ? board->target : 0;
}

uint16_t furi_hal_version_get_hw_body(void) {
    const FuriHalOtpBlock1Data* board = furi_hal_version_get_board();
    return board ? board->body : 0;
}

uint16_t furi_hal_version_get_hw_connect(void) {
    const FuriHalOtpBlock1Data* board = furi_hal_version_get_board();
    return board ? board->connect : 0;
}

uint64_t furi_hal_version_get_hw_timestamp(void) {
    const FuriHalOtpBlock1Data* board = furi_hal_version_get_board();
    return board ? board->header.timestamp : 0;
}

const char* furi_hal_version_get_name_ptr(void) {
    const FuriHalOtpBlock2Data* device = furi_hal_version_get_device();
    // NUL terminated: scripts/otp.py keeps it to 31 chars, and the CRC holds
    return device && memchr(device->name, '\0', sizeof(device->name)) ? device->name : NULL;
}

size_t furi_hal_version_uid_size(void) {
    return PICO_UNIQUE_BOARD_ID_SIZE_BYTES;
}

const uint8_t* furi_hal_version_uid(void) {
    static pico_unique_board_id_t board_id;
    pico_get_unique_board_id(&board_id);
    return board_id.id;
}

void furi_hal_version_get_uid_str(FuriString* serial) {
    char usbd_serial_str[PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 1];
    pico_get_unique_board_id_string(usbd_serial_str, sizeof(usbd_serial_str));
    furi_string_printf(serial, "%s", usbd_serial_str);
}
