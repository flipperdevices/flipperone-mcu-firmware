/**
 * @file test_test.c
 * @brief Tests the tests. These tests tests should pass the tests.
 */

#include "../unit_tests.h"

MU_TEST(test_test_basic) {
    mu_assert_int_eq(1, 1);
    mu_assert_int_not_eq(1, 2);
}

MU_TEST_SUITE(test_test_suite) {
    MU_RUN_TEST(test_test_basic);
}

int run_minunit_test_test(void) {
    MU_RUN_SUITE(test_test_suite);
    return MU_EXIT_CODE;
}
