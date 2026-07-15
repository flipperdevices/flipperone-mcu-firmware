#include "unit_test_app.h"
#include <furi.h>

// include test headers here
//#include <ucsi_ppm_test.h>
#include "tests/circular_buffer_test.h"

#define TAG "UnitTest"

// add new tests here
static const TestEntry tests[] = {
    //TEST_ENTRY(ucsi_ppm_test_run),
    TEST_ENTRY(circular_buffer_test_run),
};

static const size_t test_count = COUNT_OF(tests);

int32_t unit_test_app(void* p) {
    UNUSED(p);

    FURI_LOG_RAW_W ("\r\n");
    FURI_LOG_W(TAG, "Unit Test App started");

    unsigned failed = 0;
    for(size_t i = 0; i < test_count; ++i) {
        if(!tests[i].fn()) {
            FURI_LOG_E(TAG, "FAIL: %s", tests[i].name);
            failed++;
        }
    }

    if(failed == 0) {
        FURI_LOG_I(TAG, "All tests passed");
    } else {
        FURI_LOG_E(TAG, "%u tests failed", failed);
    }

    FURI_LOG_W(TAG, "Unit Test App finished\r\n");

    return failed == 0 ? 0 : -1;
}