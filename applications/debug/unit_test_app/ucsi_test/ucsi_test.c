/**
 * @file ucsi_test.c
 * @brief Runs the UCSI PPM core suite (lib/usb/src/ucsi_ppm_test.c) under minunit.
 */

#include "../unit_tests.h"
#include <ucsi_ppm_test.h>

MU_TEST(ucsi_ppm_suite) {
    // The suite logs every failing assertion with file and line on its own;
    // to minunit it is one pass/fail, so a failure stops the whole run.
    mu_check(ucsi_ppm_test_run());
}

MU_TEST_SUITE(ucsi_test_suite) {
    MU_RUN_TEST(ucsi_ppm_suite);
}

int run_ucsi_ppm_test(void) {
    MU_RUN_SUITE(ucsi_test_suite);
    return MU_EXIT_CODE;
}
