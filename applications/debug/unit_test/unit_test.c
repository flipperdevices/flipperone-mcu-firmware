#include <furi.h>
#include <ucsi_ppm_test.h>

#define TAG "UnitTest"

int32_t unit_test_app(void* p) {
    UNUSED(p);
    
    bool success = ucsi_ppm_test_run();

    if(success) {
        FURI_LOG_I(TAG, "All tests passed!");
    } else {
        FURI_LOG_E(TAG, "Some tests failed!");
    }

    return 0;
}
