/**
 * @file nvm_test.c
 * @brief Tests for NVM (non-volatile memory) storage API.
 */

#include "../unit_tests.h"
#include <furi_hal_nvm.h>
#include <string.h>

/* ── Test keys (deleted by teardown — excludes keys already deleted by tests) ─ */
static const char* _test_keys[] = {
    "ut_int32", "ut_uint32", "ut_bool", "ut_str", "ut_struct",
};
#define _TEST_KEY_COUNT COUNT_OF(_test_keys)

static void _nvm_teardown(void) {
    for(size_t i = 0; i < _TEST_KEY_COUNT; ++i) {
        furi_hal_nvm_delete(_test_keys[i]);
    }
}

/* ── Scalar tests ───────────────────────────────────────────────────────── */

MU_TEST(nvm_int32_set_get) {
    FuriHalNvmStorage res;
    int32_t value = -123456;

    res = furi_hal_nvm_set_int32("ut_int32", value);
    mu_assert_int_eq(FuriHalNvmStorageOK, res);

    int32_t read = 0;
    res = furi_hal_nvm_get_int32("ut_int32", &read);
    mu_assert_int_eq(FuriHalNvmStorageOK, res);
    mu_assert_int_eq(value, read);
}

MU_TEST(nvm_uint32_set_get) {
    FuriHalNvmStorage res;
    uint32_t value = 123456;

    res = furi_hal_nvm_set_uint32("ut_uint32", value);
    mu_assert_int_eq(FuriHalNvmStorageOK, res);

    uint32_t read = 0;
    res = furi_hal_nvm_get_uint32("ut_uint32", &read);
    mu_assert_int_eq(FuriHalNvmStorageOK, res);
    mu_assert_int_eq(value, read);
}

MU_TEST(nvm_bool_set_get) {
    FuriHalNvmStorage res;

    res = furi_hal_nvm_set_bool("ut_bool", true);
    mu_assert_int_eq(FuriHalNvmStorageOK, res);

    bool read = false;
    res = furi_hal_nvm_get_bool("ut_bool", &read);
    mu_assert_int_eq(FuriHalNvmStorageOK, res);
    mu_assert_int_eq(true, (int)read);
}

MU_TEST(nvm_str_set_get) {
    FuriHalNvmStorage res;
    FuriString* str = furi_string_alloc();
    furi_string_set_str(str, "Hello, NVM!");

    res = furi_hal_nvm_set_str("ut_str", str);
    mu_assert_int_eq(FuriHalNvmStorageOK, res);

    FuriString* read = furi_string_alloc();
    res = furi_hal_nvm_get_str("ut_str", read);
    mu_assert_int_eq(FuriHalNvmStorageOK, res);
    mu_assert_string_eq(furi_string_get_cstr(str), furi_string_get_cstr(read));

    furi_string_free(str);
    furi_string_free(read);
}

/* ── Struct test ─────────────────────────────────────────────────────────── */

typedef struct {
    int32_t a;
    uint32_t b;
    float c;
    uint8_t d[8];
} TestStruct;

MU_TEST(nvm_struct_set_get) {
    FuriHalNvmStorage res;
    TestStruct s = {.a = -42, .b = 0xDEADBEEF, .c = 3.14f, .d = "KVstore!"};

    res = furi_hal_nvm_set_struct("ut_struct", &s, sizeof(s));
    mu_assert_int_eq(FuriHalNvmStorageOK, res);

    TestStruct r;
    memset(&r, 0, sizeof(r));
    res = furi_hal_nvm_get_struct("ut_struct", &r, sizeof(r));
    mu_assert_int_eq(FuriHalNvmStorageOK, res);

    mu_assert_int_eq(s.a, r.a);
    mu_assert_int_eq(s.b, r.b);
    mu_assert_mem_eq(s.d, r.d, sizeof(s.d));
    /* float comparison with epsilon */
    mu_assert_double_eq(s.c, r.c);
}

/* ── Delete / not-found ─────────────────────────────────────────────────── */

MU_TEST(nvm_delete) {
    FuriHalNvmStorage res;

    res = furi_hal_nvm_set_int32("ut_del", 42);
    mu_assert_int_eq(FuriHalNvmStorageOK, res);

    res = furi_hal_nvm_delete("ut_del");
    mu_assert_int_eq(FuriHalNvmStorageOK, res);

    int32_t read = 0;
    res = furi_hal_nvm_get_int32("ut_del", &read);
    mu_assert_int_eq(FuriHalNvmStorageItemNotFound, res);
}

MU_TEST(nvm_not_found) {
    FuriHalNvmStorage res;
    int32_t read = 0;

    res = furi_hal_nvm_get_int32("ut_no_such_key", &read);
    mu_assert_int_eq(FuriHalNvmStorageItemNotFound, res);
}

/* ── Suite ──────────────────────────────────────────────────────────────── */

MU_TEST_SUITE(nvm_suite) {
    MU_RUN_TEST(nvm_int32_set_get);
    MU_RUN_TEST(nvm_uint32_set_get);
    MU_RUN_TEST(nvm_bool_set_get);
    MU_RUN_TEST(nvm_str_set_get);
    MU_RUN_TEST(nvm_struct_set_get);
    MU_RUN_TEST(nvm_delete);
    MU_RUN_TEST(nvm_not_found);
}

int run_nvm_test(void) {
    MU_RUN_SUITE(nvm_suite);
    /* Clean up all test keys once after the entire suite. */
    _nvm_teardown();
    return MU_EXIT_CODE;
}
