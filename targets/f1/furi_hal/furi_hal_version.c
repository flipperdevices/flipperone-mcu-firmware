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

// OTP_DEVICE, read once at start; all zero when the device has none
static FuriHalOtpDevice furi_hal_version_otp;
static bool furi_hal_version_otp_valid;

void furi_hal_version_init(void) {
    furi_hal_version_otp_valid = furi_hal_otp_device_read(&furi_hal_version_otp);
    if(furi_hal_version_otp_valid) {
        FURI_LOG_I(
            TAG,
            "%s: version %u, F%uB%uC%u",
            furi_hal_version_otp.name,
            furi_hal_version_otp.version,
            furi_hal_version_otp.target,
            furi_hal_version_otp.body,
            furi_hal_version_otp.connect);
    } else {
        FURI_LOG_W(TAG, "No device info in OTP");
    }
}

FuriHalVersionOtpVersion furi_hal_version_get_otp_version(void) {
    // OTP_DEVICE has a single layout so far, its version 1
    return furi_hal_version_otp_valid ? FuriHalVersionOtpVersion1 : FuriHalVersionOtpVersionEmpty;
}

uint16_t furi_hal_version_get_hw_version(void) {
    return furi_hal_version_otp.version;
}

uint16_t furi_hal_version_get_hw_target(void) {
    return furi_hal_version_otp.target;
}

uint16_t furi_hal_version_get_hw_body(void) {
    return furi_hal_version_otp.body;
}

uint16_t furi_hal_version_get_hw_connect(void) {
    return furi_hal_version_otp.connect;
}

uint64_t furi_hal_version_get_hw_timestamp(void) {
    return furi_hal_version_otp.timestamp;
}

const char* furi_hal_version_get_name_ptr(void) {
    return furi_hal_version_otp_valid ? furi_hal_version_otp.name : NULL;
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
