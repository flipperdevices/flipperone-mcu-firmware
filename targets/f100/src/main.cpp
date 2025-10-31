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
#include <drivers/ws2812/ws2812.h>
#include <strings.h>

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
    //printf("Key1 pressed!");
    furi_hal_gpio_write(&gpio_pico_led, true);
    furi_hal_gpio_write(&gpio_pico_led, false);
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

    FuriHalPwm* pwm = furi_hal_pwm_init(&gpio_key_back, 8, 200000, false);
    uint8_t duty = 0;

    GpioPin* ws2812_pins = (GpioPin*)malloc(sizeof(GpioPin) * 1);
    ws2812_pins[0] = gpio_status_led_line1;
    Ws2812* ws2812 = ws2812_init(ws2812_pins, 1);
    free(ws2812_pins);

    DisplayJd9853* display = display_jd9853_init();
    display_jd9853_fill(display, 0x00, 0x00, 0xFF); // Fill blue
    uint8_t index_led = 0;
    
    while(true) {
        furi_hal_gpio_write(&gpio_pico_led, true);
        vTaskDelay(pdMS_TO_TICKS(10));
        furi_hal_gpio_write(&gpio_pico_led, false);
        vTaskDelay(pdMS_TO_TICKS(10));
        display_jd9853_fill(display, 0x00, 0x00, 0xFF); // Fill blue
        vTaskDelay(pdMS_TO_TICKS(10));
        display_jd9853_fill(display, 0x00, 0xFF, 0xFF); // Fill cyan
        vTaskDelay(pdMS_TO_TICKS(10));
        display_jd9853_fill(display, 0xFF, 0x00, 0xFF); // Fill magenta
        vTaskDelay(pdMS_TO_TICKS(10));
        display_jd9853_fill(display, 0xFF, 0xFF, 0x00); // Fill yellow
        vTaskDelay(pdMS_TO_TICKS(10));
        display_jd9853_fill(display, 0x00, 0x00, 0x00); // Fill black

        furi_hal_pwm_set_duty_cycle(pwm, duty);
        duty += 5;
        for (size_t i = 0; i < 29; i++){
            if(index_led == i){
                ws2812_put_pixel_rgb(ws2812, 0, duty, 0, 255 - duty);
                ws2812_put_pixel_rgb(ws2812, 0, 255 - duty, 0, duty);
                ws2812_put_pixel_rgb(ws2812, 0, 255 - duty, duty, 0);
            } else {
                ws2812_put_pixel_rgb(ws2812, 0, 0, 0, 0);
            }  
        }
        index_led++;
        if(index_led >= 30) {
            index_led = 0;
        }

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
