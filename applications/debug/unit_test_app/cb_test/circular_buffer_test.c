/**
 * @file circular_buffer_test.c
 * @brief Tests for CircularBuffer (toolbox/circular_buffer.h).
 */

#include "../unit_tests.h"
#include <toolbox/circular_buffer.h>
#include <string.h>

/* ── overwrite=true ─────────────────────────────────────────────────────── */

MU_TEST(cb_overwrite_true_basic) {
    CircularBuffer* cb = circular_buffer_alloc(8, true);

    const char* test_str = "Hello";
    uint8_t data[16];
    for(size_t i = 0; i < 8; ++i) {
        size_t n = circular_buffer_write(cb, (const uint8_t*)test_str, strlen(test_str));
        mu_assert_int_eq(strlen(test_str), n);

        memset(data, 0, sizeof(data));
        n = circular_buffer_read(cb, data, sizeof(data));
        mu_assert_int_eq(strlen(test_str), n);
        mu_assert_string_eq(test_str, (const char*)data);
    }

    circular_buffer_free(cb);
}

MU_TEST(cb_overwrite_true_overflow) {
    CircularBuffer* cb = circular_buffer_alloc(8, true);

    char long_str[32];
    uint8_t data[16];
    for(size_t i = 0; i < 8; ++i) {
        size_t len_str = 8 + 1 + i; /* 9, 10, 11, ... */
        for(size_t j = 0; j < len_str; ++j)
            long_str[j] = '0' + (j % 10);
        long_str[len_str] = '\0';

        size_t n = circular_buffer_write(cb, (const uint8_t*)long_str, len_str);
        mu_assert_int_eq(len_str, n);

        memset(data, 0, sizeof(data));
        n = circular_buffer_read(cb, data, sizeof(data));
        mu_assert_int_eq(8, n);
        /* overwrite=true: last `buffer_size` bytes are kept */
        mu_assert_mem_eq(long_str + (len_str - 8), data, 8);
    }

    circular_buffer_free(cb);
}

MU_TEST(cb_overwrite_true_max_write) {
    CircularBuffer* cb = circular_buffer_alloc(8, true);
    const char* max_str = "12345678";
    uint8_t data[16];

    size_t n = circular_buffer_write(cb, (const uint8_t*)max_str, strlen(max_str));
    mu_assert_int_eq(strlen(max_str), n);

    memset(data, 0, sizeof(data));
    n = circular_buffer_read(cb, data, sizeof(data));
    mu_assert_int_eq(strlen(max_str), n);
    mu_assert_string_eq(max_str, (const char*)data);

    circular_buffer_free(cb);
}

MU_TEST(cb_overwrite_true_empty_read) {
    CircularBuffer* cb = circular_buffer_alloc(8, true);
    uint8_t data[16];

    size_t n = circular_buffer_read(cb, data, sizeof(data));
    mu_assert_int_eq(0, n);

    circular_buffer_free(cb);
}

MU_TEST(cb_overwrite_true_fifo_accumulate) {
    CircularBuffer* cb = circular_buffer_alloc(8, true);
    uint8_t data[16];

    const char* short_str = "abc#1234412";
    for(size_t i = 0; i < 5; ++i) {
        size_t n = circular_buffer_write(cb, (const uint8_t*)short_str, strlen(short_str));
        mu_assert_int_eq(strlen(short_str), n);
    }

    memset(data, 0, sizeof(data));
    size_t n = circular_buffer_read(cb, data, sizeof(data));
    mu_assert_int_eq(8, n);

    /* Build expected: last 8 bytes of 5×"abc#1234412" = "1234412a bc#1234412 abc#1234412 abc#1234412 abc#1234412"
       = "...#1234412" = last 8: "234412ab" wait let me compute properly.
       5 * 10 = 50 bytes total. Last 8 bytes of 50-char string:
       Positions: 0-49. Last 8: positions 42-49.
       "abc#1234412abc#1234412abc#1234412abc#1234412abc#1234412"
       42=a, 43=b, 44=c, 45=#, 46=1, 47=2, 48=3, 49=4
       → "abc#1234" */
    char full[64] = {0};
    for(size_t i = 0; i < 5; ++i)
        strcat(full, short_str);
    mu_assert_mem_eq(full + strlen(full) - 8, data, 8);

    circular_buffer_free(cb);
}

/* ── overwrite=false ────────────────────────────────────────────────────── */

MU_TEST(cb_overwrite_false_basic) {
    CircularBuffer* cb = circular_buffer_alloc(8, false);

    const char* test_str = "Hello";
    uint8_t data[16];
    for(size_t i = 0; i < 8; ++i) {
        size_t n = circular_buffer_write(cb, (const uint8_t*)test_str, strlen(test_str));
        mu_assert_int_eq(strlen(test_str), n);

        memset(data, 0, sizeof(data));
        n = circular_buffer_read(cb, data, sizeof(data));
        mu_assert_int_eq(strlen(test_str), n);
        mu_assert_string_eq(test_str, (const char*)data);
    }

    circular_buffer_free(cb);
}

MU_TEST(cb_overwrite_false_overflow) {
    CircularBuffer* cb = circular_buffer_alloc(8, false);

    char long_str[32];
    uint8_t data[16];
    for(size_t i = 0; i < 8; ++i) {
        size_t len_str = 8 + 1 + i; /* 9, 10, 11, ... */
        for(size_t j = 0; j < len_str; ++j)
            long_str[j] = '0' + (j % 10);
        long_str[len_str] = '\0';

        size_t n = circular_buffer_write(cb, (const uint8_t*)long_str, len_str);
        mu_assert_int_eq(8, n); /* only buffer_size bytes accepted */

        memset(data, 0, sizeof(data));
        n = circular_buffer_read(cb, data, sizeof(data));
        mu_assert_int_eq(8, n);
        /* overwrite=false: first `buffer_size` bytes are kept */
        mu_assert_mem_eq(long_str, data, 8);
    }

    circular_buffer_free(cb);
}

MU_TEST(cb_overwrite_false_fifo_accumulate) {
    CircularBuffer* cb = circular_buffer_alloc(8, false);

    const char* short_str = "a";
    for(size_t i = 0; i < 5; ++i) {
        size_t n = circular_buffer_write(cb, (const uint8_t*)short_str, strlen(short_str));
        mu_assert_int_eq(strlen(short_str), n);
    }

    size_t avail = circular_buffer_bytes_available(cb);
    mu_assert_int_eq(5, avail);

    uint8_t data[16];
    memset(data, 0, sizeof(data));
    size_t n = circular_buffer_read(cb, data, sizeof(data));
    mu_assert_int_eq(avail, n);

    circular_buffer_free(cb);
}

/* ── Suite ──────────────────────────────────────────────────────────────── */

MU_TEST_SUITE(circular_buffer_suite) {
    MU_RUN_TEST(cb_overwrite_true_basic);
    MU_RUN_TEST(cb_overwrite_true_overflow);
    MU_RUN_TEST(cb_overwrite_true_max_write);
    MU_RUN_TEST(cb_overwrite_true_empty_read);
    MU_RUN_TEST(cb_overwrite_true_fifo_accumulate);

    MU_RUN_TEST(cb_overwrite_false_basic);
    MU_RUN_TEST(cb_overwrite_false_overflow);
    MU_RUN_TEST(cb_overwrite_false_fifo_accumulate);
}

int run_circular_buffer_test(void) {
    MU_RUN_SUITE(circular_buffer_suite);
    return MU_EXIT_CODE;
}
