/**
 * @file parallax_test.c
 * @brief Tests for the parallax wrap arithmetic (services/gui/modules/parallax.h).
 */

#include "../unit_tests.h"
#include <gui/modules/parallax.h>

/* Period wrap: a 73 px sprite on a 256 px scene, period 329. */
MU_TEST(parallax_period_wrap) {
    mu_assert_int_eq(99, parallax_period_x(99, 73, 329));
    /* Leaving through the right edge... */
    mu_assert_int_eq(255, parallax_period_x(255, 73, 329));
    /* ...and once fully out (x = 256) it is just entering from the left. */
    mu_assert_int_eq(-73, parallax_period_x(256, 73, 329));
    mu_assert_int_eq(-72, parallax_period_x(257, 73, 329));
    /* Negative unwrapped positions wrap the same way. */
    mu_assert_int_eq(-1, parallax_period_x(-1, 73, 329));
    mu_assert_int_eq(255, parallax_period_x(-74, 73, 329));
    mu_assert_int_eq(99, parallax_period_x(99 - 329 * 3, 73, 329));
}

/* Tile wrap: the first copy of a 256 px tile that still reaches the clip's left edge. */
MU_TEST(parallax_tile_first) {
    mu_assert_int_eq(0, parallax_tile_first_x(0, 256, 0));
    mu_assert_int_eq(-250, parallax_tile_first_x(6, 256, 0));
    mu_assert_int_eq(-6, parallax_tile_first_x(-6, 256, 0));
    mu_assert_int_eq(-6, parallax_tile_first_x(-6 - 256 * 4, 256, 0));
    mu_assert_int_eq(0, parallax_tile_first_x(256, 256, 0));
    /* With a clip that does not start at 0 the result stays in (left - width, left]. */
    mu_assert_int_eq(-246, parallax_tile_first_x(10, 256, 1));
    mu_assert_int_eq(1, parallax_tile_first_x(1, 256, 1));
}

/* Mirroring a 113 px frame box in place: the box edges swap, a 4 px bubble at
 * x = 99 ends up 4 px from the other edge. */
MU_TEST(parallax_mirror) {
    mu_assert_int_eq(0, parallax_mirror_x(0, 113, 113));
    mu_assert_int_eq(1, parallax_mirror_x(0, 112, 113));
    mu_assert_int_eq(10, parallax_mirror_x(99, 4, 113));
    mu_assert_int_eq(99, parallax_mirror_x(10, 4, 113));
}

MU_TEST_SUITE(parallax_suite) {
    MU_RUN_TEST(parallax_period_wrap);
    MU_RUN_TEST(parallax_tile_first);
    MU_RUN_TEST(parallax_mirror);
}

int run_parallax_test(void) {
    MU_RUN_SUITE(parallax_suite);
    return MU_EXIT_CODE;
}
