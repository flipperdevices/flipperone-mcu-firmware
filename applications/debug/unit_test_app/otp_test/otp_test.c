/**
 * @file otp_test.c
 * @brief Tests for the device name in OTP. These only read OTP: an OTP write
 * cannot be undone, so nothing here calls furi_hal_otp_write_device_name().
 */

#include "../unit_tests.h"
#include <furi_hal_otp.h>
#include <furi_hal_version.h>
#include <string.h>

MU_TEST(otp_device_name_valid) {
    mu_check(furi_hal_otp_device_name_is_valid("P00per"));
    mu_check(furi_hal_otp_device_name_is_valid("a"));
    mu_check(furi_hal_otp_device_name_is_valid("0"));
    mu_check(furi_hal_otp_device_name_is_valid("Abcdefgh"));
}

MU_TEST(otp_device_name_invalid) {
    mu_check(!furi_hal_otp_device_name_is_valid(""));
    mu_check(!furi_hal_otp_device_name_is_valid("Abcdefghi"));
    mu_check(!furi_hal_otp_device_name_is_valid("bad-name"));
    mu_check(!furi_hal_otp_device_name_is_valid("a b"));
    mu_check(!furi_hal_otp_device_name_is_valid("a_b"));
    mu_check(!furi_hal_otp_device_name_is_valid("caf\xc3\xa9"));
}

MU_TEST(otp_device_name_matches_version) {
    char name[FURI_HAL_VERSION_ARRAY_NAME_LENGTH];
    const bool programmed = furi_hal_otp_read_device_name(name, sizeof(name));
    const char* version_name = furi_hal_version_get_name_ptr();

    if(programmed) {
        mu_check(version_name != NULL);
        mu_assert_string_eq(name, version_name);
    } else {
        mu_check(version_name == NULL);
        mu_assert_string_eq("", name);
    }
}

MU_TEST_SUITE(otp_suite) {
    MU_RUN_TEST(otp_device_name_valid);
    MU_RUN_TEST(otp_device_name_invalid);
    MU_RUN_TEST(otp_device_name_matches_version);
}

int run_otp_test(void) {
    MU_RUN_SUITE(otp_suite);
    return MU_EXIT_CODE;
}
