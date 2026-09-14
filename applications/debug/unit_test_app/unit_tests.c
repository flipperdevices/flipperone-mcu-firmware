#include "unit_tests.h"
#include <furi.h>
#include <cli/cli_command.h>
#include <cli/cli_ansi.h>
#include "minunit_vars.h"
#include "test_list.h"
#include <FreeRTOSConfig.h>

void unit_tests_cli_command(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    UNUSED(args);
    UNUSED(context);
    minunit_run = 0;
    minunit_assert = 0;
    minunit_fail = 0;
    minunit_status = 0;

    int32_t heap_before = memmgr_get_free_heap();
    uint32_t time_before = furi_get_tick();

    for(size_t i = 0; i < UNIT_TEST_COUNT; i++) {
        TestCallback run_test = unit_test_callbacks[i];
        run_test();
        if(minunit_fail) break;
    }

    uint32_t time_after = furi_get_tick();
    uint32_t time_delta = time_after - time_before;

    furi_delay_ms(200);
    int32_t heap_after = memmgr_get_free_heap();
    int32_t heap_delta = heap_after - heap_before;
    uint32_t heap_leaked = MAX(0, heap_delta);

    static_assert(configTICK_RATE_HZ_RAW == 1000);
    printf("Consumed: %lu ms\r\n", time_delta);

    if(heap_leaked) printf("Leaked: %lu\r\n", heap_leaked);

    printf("Status: ");
    if(minunit_fail || heap_leaked) {
        printf(ANSI_FG_BR_RED "FAILED");
    } else {
        printf(ANSI_FG_BR_GREEN "PASSED");
    }
    printf(ANSI_RESET "\r\n");
}
