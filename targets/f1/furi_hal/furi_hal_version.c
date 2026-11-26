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

// OTP blocks, read for the getters here. Written once, so a copy stays true
// once it is there; until then every call looks again, so a block written
// while running shows at once.
static FuriHalOtpBlock1Data furi_hal_version_board;
static FuriHalOtpBlock2Data furi_hal_version_device;
static FuriHalOtpBlock3Data furi_hal_version_signatures;
static volatile bool furi_hal_version_otp_read[FuriHalOtpBlockCount + 1];

static const void* furi_hal_version_get_otp_block(FuriHalOtpBlock block) {
    static void* const copies[FuriHalOtpBlockCount + 1] = {
        [FuriHalOtpBlock1] = &furi_hal_version_board,
        [FuriHalOtpBlock2] = &furi_hal_version_device,
        [FuriHalOtpBlock3] = &furi_hal_version_signatures,
    };
    furi_check(block >= FuriHalOtpBlock1 && block <= FuriHalOtpBlockCount);
    if(!furi_hal_version_otp_read[block]) {
        uint8_t data[FURI_HAL_OTP_BLOCK_MAX_SIZE];
        const size_t size = furi_hal_otp_block_size(block);
        if(furi_hal_otp_block_read(block, data, size)) {
            // Published whole: readers see the flag only after the copy
            memcpy(copies[block], data, size);
            __DMB();
            furi_hal_version_otp_read[block] = true;
        }
    }
    return furi_hal_version_otp_read[block] ? copies[block] : NULL;
}

static const FuriHalOtpBlock1Data* furi_hal_version_get_board(void) {
    return furi_hal_version_get_otp_block(FuriHalOtpBlock1);
}

static const FuriHalOtpBlock2Data* furi_hal_version_get_device(void) {
    return furi_hal_version_get_otp_block(FuriHalOtpBlock2);
}

bool furi_hal_version_is_otp_valid(FuriHalOtpBlock block) {
    return furi_hal_version_get_otp_block(block) != NULL;
}

bool furi_hal_version_is_otp_signed(void) {
    return furi_hal_otp_valid();
}

uint64_t furi_hal_version_get_otp_timestamp(FuriHalOtpBlock block) {
    const FuriHalOtpHeader* header = furi_hal_version_get_otp_block(block);
    return header ? header->timestamp : 0;
}

const uint8_t* furi_hal_version_get_otp_chip_id(void) {
    const FuriHalOtpBlock1Data* board = furi_hal_version_get_board();
    return board ? board->chip_id : NULL;
}

const uint8_t* furi_hal_version_get_mac(FuriHalOtpMac mac) {
    furi_check(mac < FuriHalOtpMacCount);
    const FuriHalOtpBlock2Data* device = furi_hal_version_get_device();
    return device ? device->mac[mac] : NULL;
}

const uint8_t* furi_hal_version_get_cpu_id(void) {
    const FuriHalOtpBlock2Data* device = furi_hal_version_get_device();
    return device ? device->cpu_id : NULL;
}

uint32_t furi_hal_version_get_ram_gb(void) {
    const FuriHalOtpBlock2Data* device = furi_hal_version_get_device();
    return device ? device->ram_gb : 0;
}

uint32_t furi_hal_version_get_ufs_gb(void) {
    const FuriHalOtpBlock2Data* device = furi_hal_version_get_device();
    return device ? device->ufs_gb : 0;
}

uint16_t furi_hal_version_get_hw_color(void) {
    const FuriHalOtpBlock2Data* device = furi_hal_version_get_device();
    return device ? device->color : 0;
}

uint16_t furi_hal_version_get_hw_display(void) {
    const FuriHalOtpBlock2Data* device = furi_hal_version_get_device();
    return device ? device->display : 0;
}

const char* furi_hal_version_get_hw_region_name(void) {
    static char region[3];
    const FuriHalOtpBlock2Data* device = furi_hal_version_get_device();
    if(!device) return "Unknown";
    memcpy(region, device->region, 2);
    return region;
}

const char* furi_hal_version_get_model_ptr(void) {
    const FuriHalOtpBlock2Data* device = furi_hal_version_get_device();
    return device && memchr(device->model, '\0', sizeof(device->model)) ? device->model : "Unknown";
}

uint16_t furi_hal_version_get_sign_key_id(void) {
    const FuriHalOtpBlock3Data* signatures = furi_hal_version_get_otp_block(FuriHalOtpBlock3);
    return signatures ? signatures->key_id : 0;
}

const uint8_t* furi_hal_version_get_otp_signature(FuriHalOtpBlock block) {
    const FuriHalOtpBlock3Data* signatures = furi_hal_version_get_otp_block(FuriHalOtpBlock3);
    if(!signatures) return NULL;
    if(block == FuriHalOtpBlock1) return signatures->block1_signature;
    if(block == FuriHalOtpBlock2) return signatures->block2_signature;
    return NULL;
}

void furi_hal_version_init(void) {
    const FuriHalOtpBlock1Data* board = furi_hal_version_get_board();
    if(board) {
        FURI_LOG_I(TAG, "Board: version %u, F%uB%uC%u", board->hw_version, board->target, board->body, board->connect);
        // Two ECDSA checks, about 140 ms; cached for furi_hal_otp_valid() callers
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
    return furi_hal_version_get_otp_timestamp(FuriHalOtpBlock1);
}

const char* furi_hal_version_get_hw_version_code(void) {
    static char code[32];
    const FuriHalOtpBlock1Data* board = furi_hal_version_get_board();
    if(!board) return "Unknown";
    snprintf(code, sizeof(code), "%u.F%u.B%u.C%u", board->hw_version, board->target, board->body, board->connect);
    return code;
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
