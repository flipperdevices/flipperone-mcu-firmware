#include <furi_hal_version.h>
//#include <furi_hal_rtc.h>

//#include <stm32u5xx_ll_utils.h>

#include <furi.h>

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
    // uid[0] = LL_GetUID_Word0(); // X and Y coordinates on the wafer expressed in BCD format
    // uid[1] = LL_GetUID_Word1(); // Wafer number and lot number
    // uid[2] = LL_GetUID_Word2(); // Lot number (ASCII encoded)

    // // Generate a unique MAC address based on the UID
    // ble_mac[3] = (uid[0] >> 16) & 0xFF;
    // ble_mac[4] = uid[0] & 0xFF;
    // ble_mac[5] = (uid[1] >> 16) & 0xFF;

    return ble_mac;
}

uint8_t furi_hal_version_get_hw_target(void) {
    return version_get_target(version_get());
}

void furi_hal_version_get_uid_str(FuriString* serial) {
    // furi_string_printf(
    //     serial, "%08lx%08lx%08lx", LL_GetUID_Word2(), LL_GetUID_Word1(), LL_GetUID_Word0());
    furi_string_printf(
        serial, "000000000000000000000000");
}
