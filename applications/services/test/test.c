#include "test.h"
#include <furi.h>

#define tag "TestSrv"

int32_t test_srv(void* p) {
    UNUSED(p);

    while(1) {
        FURI_LOG_I(tag, "Test message");
        furi_delay_ms(1000);
    }

    return 0;
}