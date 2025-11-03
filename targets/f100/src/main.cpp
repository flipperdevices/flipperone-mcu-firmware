#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_nvm.h>
#include <flipper.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"

#define TAG "Main"

int32_t init_task(void* context) {
    UNUSED(context);

    // Flipper FURI HAL\
    furi_hal_init();

    // Set the UART for logging output
    //furi_hal_serial_control_set_logging_config(FuriHalSerialIdUsart6, 230400);
    //stdio_init_all();
    FURI_LOG_I(TAG, "Init task started");

    // Init flipper
    flipper_init();

    furi_background();

    return 0;
}

int main(void) {
    //Initialize FURI layer
    stdio_init_all();
    printf("Start");
    
    furi_init();
    //todo stdio_init_all???
    

    // Critical FURI HAL
    furi_hal_init_early();

    FuriThread* main_thread = furi_thread_alloc_ex("Init", 4096, init_task, NULL);
    furi_thread_set_priority(main_thread, FuriThreadPriorityInit);
#ifdef FURI_RAM_EXEC
    furi_thread_start(main_thread);
#else
    //FuriHalNvmBootMode boot_mode = furi_hal_nvm_get_boot_mode();
    // if(boot_mode == FuriHalNvmBootModeUpdate) {
    //     furi_delay_ms(200);
    //     furi_hal_nvm_set_boot_mode(FuriHalNvmBootModeNormal);
    //     platform_boot_to_update();
    //     // If we are here, the switch to the update was not successful
    //     // FURI_LOG_W(TAG, "Failed to switch to update mode");
    //     furi_hal_power_reset();
    // } else {
    furi_thread_start(main_thread);
    //}

#endif

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
