/**
 * @file animation_test.c
 * @brief Tests for image_animation_frame_at() (services/gui/image.h): the
 *        frame an animated image shows at a given clock, and the delay the
 *        GUI arms its redraw timer with.
 */

#include "../unit_tests.h"
#include <gui/image.h>

static const uint8_t frame_pixels[1] = {0};

static const uint8_t* const frames[3] = {frame_pixels, frame_pixels, frame_pixels};

static const ImageAnimation animation = {
    .frames = frames,
    .frame_count = 3,
    .frame_duration_ms = 250,
};

MU_TEST(animation_frame_follows_clock) {
    mu_assert_int_eq(0, image_animation_frame_at(&animation, 0, NULL));
    mu_assert_int_eq(0, image_animation_frame_at(&animation, 249, NULL));
    mu_assert_int_eq(1, image_animation_frame_at(&animation, 250, NULL));
    mu_assert_int_eq(2, image_animation_frame_at(&animation, 700, NULL));
}

MU_TEST(animation_loops) {
    mu_assert_int_eq(0, image_animation_frame_at(&animation, 750, NULL));
    mu_assert_int_eq(1, image_animation_frame_at(&animation, 1000, NULL));
    /* Near the 32-bit tick wrap the sequence still just keeps counting. */
    mu_assert_int_eq((UINT32_MAX / 250) % 3, image_animation_frame_at(&animation, UINT32_MAX, NULL));
}

MU_TEST(animation_next_frame_delay) {
    uint32_t delay = 0;

    image_animation_frame_at(&animation, 0, &delay);
    mu_assert_int_eq(250, delay);

    image_animation_frame_at(&animation, 249, &delay);
    mu_assert_int_eq(1, delay);

    image_animation_frame_at(&animation, 260, &delay);
    mu_assert_int_eq(240, delay);

    /* The delay is never zero: a redraw at a frame boundary waits a full period. */
    image_animation_frame_at(&animation, 750, &delay);
    mu_assert_int_eq(250, delay);
}

MU_TEST(animation_single_frame) {
    const ImageAnimation still = {.frames = frames, .frame_count = 1, .frame_duration_ms = 100};
    uint32_t delay = 0;
    mu_assert_int_eq(0, image_animation_frame_at(&still, 12345, &delay));
    mu_assert_int_eq(55, delay);
}

MU_TEST_SUITE(animation_suite) {
    MU_RUN_TEST(animation_frame_follows_clock);
    MU_RUN_TEST(animation_loops);
    MU_RUN_TEST(animation_next_frame_delay);
    MU_RUN_TEST(animation_single_frame);
}

int run_animation_test(void) {
    MU_RUN_SUITE(animation_suite);
    return MU_EXIT_CODE;
}
