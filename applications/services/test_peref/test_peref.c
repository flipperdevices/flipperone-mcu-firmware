#include "test_peref.h"
#include <furi.h>

#include <furi_hal_resources.h>
#include <furi_hal_gpio.h>
#include <drivers/display/display_jd9853_qspi.h>

#include <furi_hal_pwm.h>
#include <drivers/ws2812/ws2812.h>

#include <furi_hal_i2c.h>
#include <drivers/tca6416a/tca6416a.h>
#include <furi_hal_power.h>
#include <drivers/drv2605l/drv2605l.h>
#include <furi_hal_i2c_config.h>
#include <drivers/iqs7211e/iqs7211e.h>
#include <drivers/spi_get_frame/spi_get_frame.h>
#include <drivers/ina219/ina219.h>
#include <stddef.h>
#include <stdint.h>
#include <drivers/tps62868x/tps62868x.h>

#define tag "TestPerefSrv"

uint8_t input_ok = 0;
uint8_t* input_data_ptr = 0;
size_t input_size = 0;

static void __isr __not_in_flash_func(rx_ok)(uint8_t* data, size_t size, void* context) {
    input_data_ptr = data;
    input_size = size;
    input_ok = 1;
}

int32_t test_peref_srv(void* p) {
    UNUSED(p);

    furi_log_set_level(FuriLogLevelDebug);
    FURI_LOG_T("tag", "Trace");
    FURI_LOG_D("tag", "Debug");
    FURI_LOG_I("tag", "Info");
    FURI_LOG_W("tag", "Warning");
    FURI_LOG_E("tag", "Error");

    uint8_t duty = 0;

    GpioPin* ws2812_pins = (GpioPin*)malloc(sizeof(GpioPin) * 3);
    ws2812_pins[0] = gpio_status_led_line1;
    ws2812_pins[1] = gpio_status_led_line2;
    ws2812_pins[2] = gpio_status_led_line3;
    Ws2812* ws2812 = ws2812_init(ws2812_pins, 3);
    free(ws2812_pins);
    
    //furi_hal_i2c_bus_scan_print(&furi_hal_i2c_handle_internal);


    DisplayJd9853QSPI* display = display_jd9853_qspi_init();
    display_jd9853_qspi_set_brightness(display, 10);
    uint8_t index_led[3] = {0};

    SpiGetFrame* spi_get_frame = spi_get_frame_init();
    spi_get_frame_set_callback_rx(spi_get_frame, rx_ok, NULL);

    //Ina219* ina219 = ina219_init(&furi_hal_i2c_handle_internal, INA219_ADDRESS, 0.1f, 0.4f); // 0.1 Ohm shunt, 2A max

    while(true) {

        // furi_delay_ms(5);
        // if(input_ok) {
        //     // FURI_LOG_RAW_I("size = %d || data:", input_size);
        //     // for(size_t i = 0; i < 32; i+=2) {
        //     //     FURI_LOG_RAW_I( " %02X%02X", input_data_ptr[i+1], input_data_ptr[i]);
        //     // }
        //     // FURI_LOG_RAW_I( "   ||   ");
        //     // for(size_t i = input_size-32; i < input_size; i+=2) {
        //     //     FURI_LOG_RAW_I( " %02X%02X", input_data_ptr[i+1], input_data_ptr[i]);
        //     // }
        //     //  FURI_LOG_RAW_I( "\r\n");
        //     //FURI_LOG_I("SPI1", "Received byte %c%c%c%c%c%c%c%c%c", input_data_ptr[0], input_data_ptr[1], input_data_ptr[2], input_data_ptr[3], input_data_ptr[4], input_data_ptr[5], input_data_ptr[6], input_data_ptr[7], input_data_ptr[8]);
        //     display_jd9853_qspi_write_buffer(display, input_data_ptr, input_size);
        //     input_ok = 0;

        // }

        // furi_hal_gpio_write(&gpio_pico_led, true);
        //  furi_delay_ms(100);
        // float bus_v = ina219_get_bus_voltage_v(ina219);
        // float current_a = ina219_get_current_a(ina219);
        // float power_w = ina219_get_power_w(ina219);
        // float shunt_mv = ina219_get_shunt_voltage_mv(ina219);
        // FURI_LOG_I("Ina219", "Bus Voltage: %.3f V | Shunt Voltage: %.6f mV | Current: %.6f A | Power: %.6f W",
        //     bus_v,
        //     shunt_mv,
        //     current_a,
        //     power_w);


        // furi_hal_gpio_write(&gpio_pico_led, false);
        // furi_delay_ms(10);

        //bw display test
        display_jd9853_qspi_fill(display, 0); // Fill white
        furi_delay_ms(200);
        // display_jd9853_qspi_fill(display, 50); // Fill white
        // furi_delay_ms(200);
        // display_jd9853_qspi_fill(display, 100); // Fill white
        // furi_delay_ms(200);
        // display_jd9853_qspi_fill(display, 150); // Fill white
        // furi_delay_ms(200);
        // display_jd9853_qspi_fill(display, 200); // Fill white
        // furi_delay_ms(200);
        display_jd9853_qspi_fill(display, 255); // Fill white
    //    furi_delay_ms(500);

    //     for(size_t i = 0; i < 64; i++) {
    //         //furi_hal_gpio_write(&gpio_display_ctrl, true);
    //         display_jd9853_qspi_fill(display, i<<2); // Fill white
    //         //furi_delay_ms(100); //10FPS
    //         //furi_delay_ms(66);  //15FPS
    //         //furi_delay_ms(50);  //20FPS
    //         // furi_delay_ms(33); //30FPS
    //          furi_delay_ms(16); //60FPS
    //         //furi_delay_ms(5); //120FPS
    //     }
    //    furi_delay_ms(200);

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
        //     display_jd9853_qspi_write_buffer_x_y(display, x0, y0, w, h, buf, (w) * (h));
        //     free(buf);
        //     furi_delay_ms(10);

        // duty += 1;
        // if(duty % 2){
        //     display_jd9853_qspi_eco_mode(display, true);
        // } else {
        //     display_jd9853_qspi_eco_mode(display, false);
        // }
        // FURI_LOG_I("backlight", "Brightness: %d", duty);
        // display_jd9853_qspi_backlight_set_brightness(display, duty);
        // if(duty >= 100) {
        //     duty = 0;
        // }
        //   //  furi_hal_power_insomnia_enter();

            //test line 1
            uint32_t line_buffer_1[4];
            for(size_t i = 0; i < sizeof(line_buffer_1)/4; i++) {
                if(index_led[0] == i) {
                    line_buffer_1[i] = ws2812_urgb_u32_dma(127,   30, 30);
                } else {
                    line_buffer_1[i] = ws2812_urgb_u32_dma(0, 0, 0);
                }
            }

            ws2812_write_buffer_dma(ws2812, 0, line_buffer_1, 4);
            index_led[0]++;
            if(index_led[0] >= sizeof(line_buffer_1)/4) {
                index_led[0] = 0;
            }

            //test line 2
            uint32_t line_buffer_2[7];
            for(size_t i = 0; i < sizeof(line_buffer_2)/4; i++) {
                if(index_led[1] == i) {
                    line_buffer_2[i] = ws2812_urgb_u32_dma(127,   30, 30);
                } else {
                    line_buffer_2[i] = ws2812_urgb_u32_dma(0, 0, 0);
                }
            }

            ws2812_write_buffer_dma(ws2812, 1, line_buffer_2, 7);
            index_led[1]++;
            if(index_led[1] >= sizeof(line_buffer_2)/4) {
                index_led[1] = 0;
            }

            //test line 3
            uint32_t line_buffer_3[6];
            for(size_t i = 0; i < sizeof(line_buffer_3)/4; i++) {
                if(index_led[2] == i) {
                    line_buffer_3[i] = ws2812_urgb_u32_dma(127,   30, 30);
                } else {
                    line_buffer_3[i] = ws2812_urgb_u32_dma(0, 0, 0);
                }
            }

            ws2812_write_buffer_dma(ws2812, 2, line_buffer_3, 6);
            index_led[2]++;
            if(index_led[2] >= sizeof(line_buffer_3)/4) {
                index_led[2] = 0;
            }

        //furi_hal_i2c_acquire(&furi_hal_i2c_handle_internal);
        // furi_hal_i2c_bus_scan_print(&furi_hal_i2c_handle_internal);
        // furi_hal_i2c_release(&furi_hal_i2c_handle_internal);
        // uint16_t input_state = tca6416a_read_input(tca6416a);
        // FURI_LOG_I(tag, "TCA6416A input state bin: %016b", input_state);
        // furi_thread_yield();
    }
    furi_crash();
}
