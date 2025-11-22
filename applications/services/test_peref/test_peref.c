#include "test_peref.h"
#include <furi.h>

#include <furi_hal_resources.h>
#include <furi_hal_gpio.h>
#include <drivers/display/display_jd9853.h>

#include <furi_hal_pwm.h>
#include <drivers/ws2812/ws2812.h>

#include <furi_hal_i2c.h>
#include <drivers/tsa6416a/tsa6416a.h>
#include <furi_hal_power.h>

#define tag "TestPerefSrv"

uint8_t input_temp =0;

// static void input_callback (void* ctx) {
//     Tsa6416a* instance = (Tsa6416a*)ctx;
//     input_temp = 1;
// }

// static void key3_callback(void* ctx) {
//     //printf("Key1 pressed!");
//     furi_hal_gpio_write(&gpio_pico_led, true);
//     furi_hal_gpio_write(&gpio_pico_led, false);
// }

int32_t test_peref_srv(void* p) {
    UNUSED(p);

    furi_log_set_level(FuriLogLevelDebug);
    FURI_LOG_T("tag", "Trace");
    FURI_LOG_D("tag", "Debug");
    FURI_LOG_I("tag", "Info");
    FURI_LOG_W("tag", "Warning");
    FURI_LOG_E("tag", "Error");

    //furi_hal_gpio_init_simple(&gpio_key_right, GpioModeOutputPushPull);
    // furi_hal_gpio_init_simple(&gpio_key3, GpioModeInput);
    // furi_hal_gpio_add_int_callback(&gpio_key3, GpioConditionFall, key3_callback, NULL);

    uint8_t duty = 0;

    GpioPin* ws2812_pins = (GpioPin*)malloc(sizeof(GpioPin) * 1);
    ws2812_pins[0] = gpio_status_led_line1;
    Ws2812* ws2812 = ws2812_init(ws2812_pins, 1);
    free(ws2812_pins);

    DisplayJd9853* display = display_jd9853_init();
    furi_delay_ms(500);
    display_jd9853_deinit(display);
    furi_delay_ms(500);
    display = display_jd9853_init();

    uint8_t index_led = 0;

    // FuriHalPwm* pwm = furi_hal_pwm_init(&gpio_display_ctrl, 8, 50000, false);
    // furi_hal_pwm_set_duty_cycle(pwm, 160);
    //furi_hal_gpio_init_simple(&gpio_display_ctrl, GpioModeOutputPushPull);
    while(true) {
        // furi_hal_gpio_write(&gpio_pico_led, true);
        // furi_delay_ms(10);
        // furi_hal_gpio_write(&gpio_pico_led, false);
        // furi_delay_ms(10);

        // //bw display test
        display_jd9853_fill(display, 0); // Fill white
        furi_delay_ms(200);
        // display_jd9853_fill(display, 50); // Fill white
        // furi_delay_ms(200);
        // display_jd9853_fill(display, 100); // Fill white
        // furi_delay_ms(200);
        // display_jd9853_fill(display, 150); // Fill white
        // furi_delay_ms(200);
        // display_jd9853_fill(display, 200); // Fill white
        // furi_delay_ms(200);
        display_jd9853_fill(display, 255); // Fill white
         furi_delay_ms(400);

        // for(size_t i = 0; i < 64; i++) {
        //     //furi_hal_gpio_write(&gpio_display_ctrl, true);
        //     display_jd9853_fill(display, i<<2); // Fill white
        //     //furi_delay_ms(100); //10FPS
        //     //furi_delay_ms(66);  //15FPS
        //     //furi_delay_ms(50);  //20FPS
        //     // furi_delay_ms(33); //30FPS
        //     // furi_delay_ms(16); //60FPS
        //      furi_delay_ms(5); //120FPS
        // }
        // furi_delay_ms(200);


    //     // //random SQUARE
    //     uint16_t x0 = rand() % 257;
    //     uint16_t y0 = rand() % 143;
    //     uint8_t w = (rand() % 25)+1;
    //     uint8_t h = (rand() % 25)+1;
    //     uint8_t color = rand() % 255;

    //    //FURI_LOG_I("TAG", "Drawing square at (%d, %d) to (%d, %d) with color %d", x0, y0, w, h, color);

    //     uint8_t* buf = (uint8_t*)malloc( (w) * (h));
    //     for(size_t i = 0; i < (w) * (h); i++) {
    //         buf[i] = color;
    //     }
    //     display_jd9853_write_buffer_x_y(display, x0, y0, w, h, buf, (w) * (h));
    //     free(buf);
    //     furi_delay_ms(10);





    //     // furi_hal_pwm_set_duty_cycle(pwm, duty);
    //     duty += 5;
    //   //  furi_hal_power_insomnia_enter();
    //   //furi_delay_ms(3);
    //     for(size_t i = 0; i < 29; i++) {
    //         if(index_led == i) {
    //             // ws2812_put_pixel_rgb(ws2812, 0, duty, 0, 255 - duty);
    //             // ws2812_put_pixel_rgb(ws2812, 0, 255 - duty, 0, duty);
    //             // ws2812_put_pixel_rgb(ws2812, 0, 255 - duty, duty, 0);
    //             ws2812_put_pixel_rgb(ws2812, 0, 255, 0, 0);
    //             ws2812_put_pixel_rgb(ws2812, 0, 0, 255, 0);
    //             ws2812_put_pixel_rgb(ws2812, 0, 0, 0, 255);
    //         } else {
    //             ws2812_put_pixel_rgb(ws2812, 0, 0, 0, 0);
    //         }
    //     }
        
    //     index_led++;
    //     if(index_led >= 30) {
    //         index_led = 0;
    //     }
    //     //todo : It is necessary to provide a sufficient delay before going to sleep so that the PIO has time to transfer data
    //     for(size_t i = 0; i < 10000; i++) {
    //         __asm__("nop"); // Delay for WS2812 timing
    //     }

       // furi_delay_ms(3);
       // furi_hal_power_insomnia_exit();
       // furi_delay_ms(100);


        //furi_hal_i2c_acquire(&furi_hal_i2c_handle_internal);
        // furi_hal_i2c_bus_scan_print(&furi_hal_i2c_handle_internal);
        // furi_hal_i2c_release(&furi_hal_i2c_handle_internal);
        // uint16_t input_state = tsa6416a_read_input(tsa6416a);
        // FURI_LOG_I(tag, "TSA6416A input state bin: %016b", input_state);
        // furi_thread_yield();

    }
    furi_crash();
}