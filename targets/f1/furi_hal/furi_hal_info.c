#include <furi.h>

#include <furi_hal_version.h>
#include <furi_hal_info.h>
#include <furi_hal_otp.h>

#include <version/version.h>
#include <toolbox/hex.h>

#define FURI_HAL_MCU_INFO_NAME "rp2350"

FURI_WEAK void furi_hal_info_get_api_version(uint16_t* major, uint16_t* minor) {
    *major = 0;
    *minor = 0;
}

static inline void format_bytes_hex(FuriString* str, const uint8_t* data, size_t len) {
    hex_bytes_to_string(data, len, str);
}

static void property_out_str(PropertyValueContext* ctx, const char* k1, const char* k2, const char* val) {
    property_value_out(ctx, NULL, 3, FURI_HAL_MCU_INFO_NAME, k1, k2, val);
}

static void property_out_hex(PropertyValueContext* ctx, FuriString* temp, const char* k1, const char* k2, const uint8_t* data, size_t size) {
    format_bytes_hex(temp, data, size);
    property_out_str(ctx, k1, k2, furi_string_get_cstr(temp));
}

static void property_out_uint(PropertyValueContext* ctx, const char* k1, const char* k2, uint32_t val) {
    property_value_out(ctx, "%lu", 3, FURI_HAL_MCU_INFO_NAME, k1, k2, val);
}

static void property_out_time(PropertyValueContext* ctx, const char* k1, uint64_t timestamp) {
    property_value_out(ctx, "%llu", 3, FURI_HAL_MCU_INFO_NAME, k1, "timestamp", (unsigned long long)timestamp);
}

// The OTP blocks: each one's fields once it is written whole
static void furi_hal_info_get_otp(PropertyValueContext* ctx, FuriString* temp) {
    static const char* const mac_names[FuriHalOtpMacCount] = {"eth0", "eth1", "usb", "wifi_2g4_sta", "wifi_5g_sta", "wifi_2g4_ap", "wifi_5g_ap", "bt"};

    FuriHalOtpBlock1Data b1;
    const bool b1_valid = furi_hal_otp_block_read(FuriHalOtpBlock1, &b1, sizeof(b1));
    property_out_str(ctx, "otp1", "valid", b1_valid ? "true" : "false");
    if(b1_valid) {
        property_out_uint(ctx, "otp1", "version", b1.hw_version);
        property_out_uint(ctx, "otp1", "target", b1.target);
        property_out_uint(ctx, "otp1", "body", b1.body);
        property_out_uint(ctx, "otp1", "connect", b1.connect);
        property_out_str(ctx, "otp1", "version_code", furi_hal_version_get_hw_version_code());
        property_out_hex(ctx, temp, "otp1", "chip_id", b1.chip_id, sizeof(b1.chip_id));
        property_out_time(ctx, "otp1", b1.header.timestamp);
    }

    FuriHalOtpBlock2Data b2;
    const bool b2_valid = furi_hal_otp_block_read(FuriHalOtpBlock2, &b2, sizeof(b2));
    property_out_str(ctx, "otp2", "valid", b2_valid ? "true" : "false");
    if(b2_valid) {
        for(size_t i = 0; i < FuriHalOtpMacCount; i++) {
            const uint8_t* m = b2.mac[i];
            furi_string_printf(temp, "%02x:%02x:%02x:%02x:%02x:%02x", m[0], m[1], m[2], m[3], m[4], m[5]);
            property_value_out(ctx, NULL, 4, FURI_HAL_MCU_INFO_NAME, "otp2", "mac", mac_names[i], furi_string_get_cstr(temp));
        }
        furi_string_printf(temp, "%.*s", (int)strnlen(b2.name, sizeof(b2.name)), b2.name);
        property_out_str(ctx, "otp2", "name", furi_string_get_cstr(temp));
        furi_string_printf(temp, "%.*s", (int)strnlen(b2.model, sizeof(b2.model)), b2.model);
        property_out_str(ctx, "otp2", "model", furi_string_get_cstr(temp));
        property_out_uint(ctx, "otp2", "color", b2.color);
        furi_string_printf(temp, "%.*s", (int)strnlen(b2.region, sizeof(b2.region)), b2.region);
        property_out_str(ctx, "otp2", "region", furi_string_get_cstr(temp));
        property_out_hex(ctx, temp, "otp2", "cpu_id", b2.cpu_id, sizeof(b2.cpu_id));
        property_out_uint(ctx, "otp2", "ram_gb", b2.ram_gb);
        property_out_uint(ctx, "otp2", "ufs_gb", b2.ufs_gb);
        property_out_uint(ctx, "otp2", "display", b2.display);
        property_out_time(ctx, "otp2", b2.header.timestamp);
    }

    FuriHalOtpBlock3Data b3;
    const bool b3_valid = furi_hal_otp_block_read(FuriHalOtpBlock3, &b3, sizeof(b3));
    property_out_str(ctx, "otp3", "valid", b3_valid ? "true" : "false");
    if(b3_valid) {
        property_out_uint(ctx, "otp3", "key_id", b3.key_id);
        property_out_hex(ctx, temp, "otp3", "otp1_sig", b3.block1_signature, sizeof(b3.block1_signature));
        property_out_hex(ctx, temp, "otp3", "otp2_sig", b3.block2_signature, sizeof(b3.block2_signature));
        property_out_time(ctx, "otp3", b3.header.timestamp);
    }

    // All three written, and the signatures verify with a key the firmware carries
    property_out_str(ctx, "otp", "valid", furi_hal_otp_valid() ? "true" : "false");
}

void furi_hal_info_get(PropertyValueCallback out, char sep, void* context) {
    UNUSED(out);
    UNUSED(sep);
    UNUSED(context);

    FuriString* key = furi_string_alloc();
    FuriString* value = furi_string_alloc();
    FuriString* temp_str = furi_string_alloc();

    PropertyValueContext property_context = {.key = key, .value = value, .out = out, .sep = sep, .last = false, .context = context};

    // Firmware version
    const Version* firmware_version = version_get();
    if(firmware_version) {
        if(sep == '.') {
            property_value_out(&property_context, NULL, 4, FURI_HAL_MCU_INFO_NAME, "firmware", "commit", "hash", version_get_githash(firmware_version));
            property_value_out(&property_context, NULL, 4, FURI_HAL_MCU_INFO_NAME, "firmware", "branch", "name", version_get_gitbranch(firmware_version));
        } else {
            property_value_out(&property_context, NULL, 3, FURI_HAL_MCU_INFO_NAME, "firmware", "commit", version_get_githash(firmware_version));
            property_value_out(&property_context, NULL, 3, FURI_HAL_MCU_INFO_NAME, "firmware", "branch", version_get_gitbranch(firmware_version));
        }

        property_value_out(
            &property_context, NULL, 4, FURI_HAL_MCU_INFO_NAME, "firmware", "commit", "dirty", version_get_dirty_flag(firmware_version) ? "true" : "false");

        property_value_out(&property_context, NULL, 3, FURI_HAL_MCU_INFO_NAME, "firmware", "version", version_get_version(firmware_version));
        property_value_out(&property_context, NULL, 3, FURI_HAL_MCU_INFO_NAME, "firmware", "builddate", version_get_builddate(firmware_version));
        property_value_out(&property_context, "%d", 3, FURI_HAL_MCU_INFO_NAME, "firmware", "target", version_get_target(firmware_version));

        uint16_t api_version_major, api_version_minor;
        furi_hal_info_get_api_version(&api_version_major, &api_version_minor);
        property_value_out(&property_context, "%d", 4, FURI_HAL_MCU_INFO_NAME, "firmware", "api", "major", api_version_major);
        property_value_out(&property_context, "%d", 4, FURI_HAL_MCU_INFO_NAME, "firmware", "api", "minor", api_version_minor);

        property_value_out(&property_context, NULL, 4, FURI_HAL_MCU_INFO_NAME, "firmware", "origin", "fork", version_get_firmware_origin(firmware_version));
        property_value_out(&property_context, NULL, 4, FURI_HAL_MCU_INFO_NAME, "firmware", "origin", "git", version_get_git_origin(firmware_version));

        // Hardware UID
        format_bytes_hex(temp_str, furi_hal_version_uid(), furi_hal_version_uid_size());
        property_out_str(&property_context, "hardware", "uid", furi_string_get_cstr(temp_str));
    }

    furi_hal_info_get_otp(&property_context, temp_str);

    furi_string_free(temp_str);
    furi_string_free(key);
    furi_string_free(value);
}
