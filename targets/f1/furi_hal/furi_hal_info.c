#include <furi.h>

#include <furi_hal_version.h>
#include <furi_hal_info.h>

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

static void property_out_bool(PropertyValueContext* ctx, const char* k1, const char* k2, bool val) {
    property_out_str(ctx, k1, k2, val ? "true" : "false");
}

static void property_out_uint(PropertyValueContext* ctx, const char* k1, const char* k2, uint32_t val) {
    property_value_out(ctx, "%lu", 3, FURI_HAL_MCU_INFO_NAME, k1, k2, val);
}

static void property_out_long(PropertyValueContext* ctx, const char* k1, const char* k2, uint64_t val) {
    property_value_out(ctx, "%llu", 3, FURI_HAL_MCU_INFO_NAME, k1, k2, (unsigned long long)val);
}

static void property_out_hex(PropertyValueContext* ctx, FuriString* temp, const char* k1, const char* k2, const uint8_t* data, size_t size) {
    if(data) {
        format_bytes_hex(temp, data, size);
    } else {
        furi_string_set(temp, "null");
    }
    property_out_str(ctx, k1, k2, furi_string_get_cstr(temp));
}

static void property_out_mac(PropertyValueContext* ctx, FuriString* temp, const char* name, const uint8_t* mac) {
    if(mac) {
        furi_string_printf(temp, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    } else {
        furi_string_set(temp, "null");
    }
    property_value_out(ctx, NULL, 4, FURI_HAL_MCU_INFO_NAME, "otp2", "mac", name, furi_string_get_cstr(temp));
}

// The OTP blocks, every key always: values of a block not written are 0,
// "Unknown" or "null"
static void furi_hal_info_get_otp(PropertyValueContext* ctx, FuriString* temp) {
    static const char* const mac_names[FuriHalOtpMacCount] = {"eth0", "eth1", "usb", "wifi_2g4_sta", "wifi_5g_sta", "wifi_2g4_ap", "wifi_5g_ap", "bt"};

    property_out_bool(ctx, "otp1", "valid", furi_hal_version_is_otp_valid(FuriHalOtpBlock1));
    property_out_uint(ctx, "otp1", "version", furi_hal_version_get_hw_version());
    property_out_uint(ctx, "otp1", "target", furi_hal_version_get_hw_target());
    property_out_uint(ctx, "otp1", "body", furi_hal_version_get_hw_body());
    property_out_uint(ctx, "otp1", "connect", furi_hal_version_get_hw_connect());
    property_out_str(ctx, "otp1", "version_code", furi_hal_version_get_hw_version_code());
    property_out_hex(ctx, temp, "otp1", "chip_id", furi_hal_version_get_otp_chip_id(), FURI_HAL_OTP_CHIP_ID_SIZE);
    property_out_long(ctx, "otp1", "timestamp", furi_hal_version_get_otp_timestamp(FuriHalOtpBlock1));

    property_out_bool(ctx, "otp2", "valid", furi_hal_version_is_otp_valid(FuriHalOtpBlock2));
    for(size_t i = 0; i < FuriHalOtpMacCount; i++) {
        property_out_mac(ctx, temp, mac_names[i], furi_hal_version_get_mac(i));
    }
    const char* name = furi_hal_version_get_name_ptr();
    property_out_str(ctx, "otp2", "name", name ? name : "Unknown");
    property_out_str(ctx, "otp2", "model", furi_hal_version_get_model_ptr());
    property_out_uint(ctx, "otp2", "color", furi_hal_version_get_hw_color());
    property_out_str(ctx, "otp2", "region", furi_hal_version_get_hw_region_name());
    property_out_hex(ctx, temp, "otp2", "cpu_id", furi_hal_version_get_cpu_id(), sizeof(((FuriHalOtpBlock2Data*)0)->cpu_id));
    property_out_uint(ctx, "otp2", "ram_gb", furi_hal_version_get_ram_gb());
    property_out_uint(ctx, "otp2", "ufs_gb", furi_hal_version_get_ufs_gb());
    property_out_uint(ctx, "otp2", "display", furi_hal_version_get_hw_display());
    property_out_long(ctx, "otp2", "timestamp", furi_hal_version_get_otp_timestamp(FuriHalOtpBlock2));

    property_out_bool(ctx, "otp3", "valid", furi_hal_version_is_otp_valid(FuriHalOtpBlock3));
    property_out_uint(ctx, "otp3", "key_id", furi_hal_version_get_sign_key_id());
    property_out_hex(ctx, temp, "otp3", "otp1_sig", furi_hal_version_get_otp_signature(FuriHalOtpBlock1), FURI_HAL_OTP_SIGN_SIZE);
    property_out_hex(ctx, temp, "otp3", "otp2_sig", furi_hal_version_get_otp_signature(FuriHalOtpBlock2), FURI_HAL_OTP_SIGN_SIZE);
    property_out_long(ctx, "otp3", "timestamp", furi_hal_version_get_otp_timestamp(FuriHalOtpBlock3));

    property_out_bool(ctx, "otp", "white_label", furi_hal_version_is_usb_white_label_valid());

    // All three written, and the signatures verify with a key the firmware
    // carries. The last property.
    ctx->last = true;
    property_out_bool(ctx, "otp", "valid", furi_hal_version_is_otp_signed());
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
