#include <furi_hal_version.h>

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

uint8_t furi_hal_version_get_hw_target(void) {
    return version_get_target(version_get());
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
