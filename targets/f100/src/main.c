#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_nvm.h>
#include <furi_bsp.h>
#include <flipper.h>

#include "pico/multicore.h"

#define TAG "Main"

int32_t init_task(void* context) {
    UNUSED(context);

    // Flipper FURI HAL
    furi_hal_init();

    // Set the UART for logging output
    furi_hal_serial_control_set_logging_config(FuriHalSerialIdUart1, 230400);
    // ToDo: set debug mode
    furi_log_set_level(FuriLogLevelInfo);

    // Flipper BSP init
    furi_bsp_init();

    FURI_LOG_I(TAG, "Init task started");

    // Init flipper
    flipper_init();

    furi_background();

    return 0;
}

int main(void) {
    //Initialize FURI layer
    furi_init();

    // Critical FURI HAL
    furi_hal_init_early();

    FuriThread* main_thread = furi_thread_alloc_ex("Init", 4096, init_task, NULL);
    furi_thread_set_priority(main_thread, FuriThreadPriorityInit);
    furi_thread_start(main_thread);

    // somehow openocd fucks up the multicore reset
    // so we need to reset core1 manually
    sleep_ms(5);
    multicore_reset_core1();
    (void)multicore_fifo_pop_blocking();

    // Run Kernel
    furi_run();

    furi_crash("Kernel is Dead");
}

void abort(void) {
    furi_crash("AbortHandler");
}
