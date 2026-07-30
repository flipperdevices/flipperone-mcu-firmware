/**
 * @file test_list.h
 * @brief Lists the tests
 */

#pragma once

#include <furi.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TEST_FUNCTION_DECLS
#include "test_test/test_test.h"
#include "cb_test/circular_buffer_test.h"
#include "nvm_test/nvm_test.h"
#undef TEST_FUNCTION_DECLS

typedef int (*TestCallback)(void);

static TestCallback unit_test_callbacks[] = {
#define TEST_FUNCTION_REFS
#include "test_test/test_test.h"
#include "cb_test/circular_buffer_test.h"
#include "nvm_test/nvm_test.h"
#undef TEST_FUNCTION_REFS
};

#define UNIT_TEST_COUNT COUNT_OF(unit_test_callbacks)

#ifdef __cplusplus
}
#endif
