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

    furi_string_free(temp_str);
    furi_string_free(key);
    furi_string_free(value);
}
