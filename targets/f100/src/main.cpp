#include <furi.h>
#include <furi_hal.h>
//#include <flipper.h>

#include <pico/types.h>
#include <stdio.h>
#include "pico/stdlib.h"
//#include <drivers/log.hpp>
#include "FreeRTOS.h"
#include "task.h"
#include "pico/multicore.h"

#include <furi_hal_resources.h>
#include <furi_hal_gpio.h>
#include <drivers/display/display_jd9853.h>
// #include <furi_hal_spi.h>
// #include <furi_hal_spi_types_i.h>

//#include <platform_startup.h>
#include <furi_hal_pwm.h>

#define TAG "Main"

// int32_t init_task(void* context) {
//     UNUSED(context);

//     // Flipper FURI HAL
//     furi_hal_init();

//     // Set the UART for logging output
//     furi_hal_serial_control_set_logging_config(FuriHalSerialIdUsart6, 230400);

//     // Init flipper
//     flipper_init();

//     furi_background();

//     return 0;
// }

static void key1_callback(void* ctx) {
    printf("Key1 pressed!");
}

static void task_main(void* arg) {
    furi_log_set_level(FuriLogLevelDebug);
    FURI_LOG_T("tag", "Trace");
    FURI_LOG_D("tag", "Debug");
    FURI_LOG_I("tag", "Info");
    FURI_LOG_W("tag", "Warning");
    FURI_LOG_E("tag", "Error");

    furi_hal_gpio_init_simple(&gpio_pico_led, GpioModeOutputPushPull);
    furi_hal_gpio_init_simple(&gpio_key1, GpioModeInput);
    furi_hal_gpio_add_int_callback(&gpio_key1, GpioConditionFall, key1_callback, NULL);

    // FuriHalSpiHandle spi_handle = {
    //     .id = FuriHalSpiIdSPI0,
    // };
    // furi_hal_spi_init(&spi_handle, 4000000, FuriHalSpiTransferMode0, FuriHalSpiTransferBitOrderMsbFirst, FuriHalSpiModeMaster);
    FuriHalPwm* pwm = furi_hal_pwm_init(&gpio_key_back, 8, 200000, false);
    uint8_t duty = 0;

    DisplayJd9853* display = display_jd9853_init();
    display_jd9853_fill(display, 0x00, 0x00, 0xFF); // Fill blue

    while(true) {
        furi_hal_gpio_write(&gpio_pico_led, true);
        vTaskDelay(pdMS_TO_TICKS(100));
        furi_hal_gpio_write(&gpio_pico_led, false);
        vTaskDelay(pdMS_TO_TICKS(100));
        display_jd9853_fill(display, 0x00, 0x00, 0xFF); // Fill blue
        vTaskDelay(pdMS_TO_TICKS(100));
        display_jd9853_fill(display, 0x00, 0xFF, 0xFF); // Fill cyan
        vTaskDelay(pdMS_TO_TICKS(100));
        display_jd9853_fill(display, 0xFF, 0x00, 0xFF); // Fill magenta
        vTaskDelay(pdMS_TO_TICKS(100));
        display_jd9853_fill(display, 0xFF, 0xFF, 0x00); // Fill yellow
        vTaskDelay(pdMS_TO_TICKS(100));
        display_jd9853_fill(display, 0x00, 0x00, 0x00); // Fill black

        furi_hal_pwm_set_duty_cycle(pwm, duty);
        duty += 5;


        // uint8_t tx_data[] = {0xAA, 0x55, 0xFF, 0x00};
        // furi_hal_spi_tx_blocking(&spi_handle, tx_data, sizeof(tx_data));
        // uint8_t tx_data1[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
        // furi_hal_spi_tx_blocking(&spi_handle, tx_data1, sizeof(tx_data1));
        // furi_hal_spi_tx_blocking(&spi_handle, tx_data, sizeof(tx_data));
        // furi_hal_spi_tx_blocking(&spi_handle, tx_data1, sizeof(tx_data1));
    }
    furi_crash();
}

int main(void) {
    // xTaskCreate(task_main, "task_main", 1024 * 8, NULL, configMAX_PRIORITIES - 1, NULL);

    // // somehow openocd fucks up the multicore reset
    // // so we need to reset core1 manually
    // sleep_ms(5);
    // multicore_reset_core1();
    // (void)multicore_fifo_pop_blocking();

    // vTaskStartScheduler();

    // /* should never reach here */
    // panic_unsupported();

    //Initialize FURI layer

    furi_init();

    //todo stdio_init_all???
    stdio_init_all();

    // Critical FURI HAL
    furi_hal_init_early();

    //     FuriThread* main_thread = furi_thread_alloc_ex("Init", 4096, init_task, NULL);
    //     furi_thread_set_priority(main_thread, FuriThreadPriorityInit);
    // #ifdef FURI_RAM_EXEC
    //     furi_thread_start(main_thread);
    // #else
    //     FuriHalNvmBootMode boot_mode = furi_hal_nvm_get_boot_mode();
    //     if(boot_mode == FuriHalNvmBootModeUpdate) {
    //         furi_delay_ms(200);
    //         furi_hal_nvm_set_boot_mode(FuriHalNvmBootModeNormal);
    //         platform_boot_to_update();
    //         // If we are here, the switch to the update was not successful
    //         // FURI_LOG_W(TAG, "Failed to switch to update mode");
    //         furi_hal_power_reset();
    //     } else {
    //         furi_thread_start(main_thread);
    //     }

    // #endif

    xTaskCreate(task_main, "task_main", 1024 * 8, NULL, configMAX_PRIORITIES - 1, NULL);

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
