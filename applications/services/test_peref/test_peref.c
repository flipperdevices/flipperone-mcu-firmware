#include "test_peref.h"
#include <furi.h>

#include <furi_hal_resources.h>
#include <furi_hal_gpio.h>

#include <furi_hal_pwm.h>
//#include <drivers/ws2812/ws2812.h>

#include <furi_hal_i2c.h>
#include <drivers/tca6416a/tca6416a.h>
#include <furi_hal_power.h>
#include <drivers/drv2605l/drv2605l.h>
#include <furi_hal_i2c_config.h>
#include <drivers/iqs7211e/iqs7211e.h>
#include <drivers/ina219/ina219.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <drivers/tps62868x/tps62868x.h>
#include <display/display.h>
#include <input/input.h>
#include <drivers/display/display_jd9853_qspi.h>
#include <drivers/display/display_jd9853_reg.h>
#include <drivers/display/jd9853_reg.h>
#include <status_lights/status_lights.h>

#define TAG "PerefTest"
DisplayJd9853QSPI* display_h = NULL;
int mod = 1;
bool eco = false;

static void input_events_callback(const void* value, void* ctx) {
    furi_assert(value);
    furi_assert(ctx);

    const InputEvent* event = value;
    if(event->type == InputTypePress) {
        if(event->key == InputKeyUp) {
            display_jd9853_qspi_set_brightness(display_h, display_jd9853_qspi_get_brightness(display_h) + 2);
        } else if(event->key == InputKeyRight) {
            float vci = display_jd9853_qspi_get_vci(display_h);
            vci += 0.1f * mod;
            if(vci < 2.0f) vci = 2.0f;
            if(vci > 3.3f) vci = 3.3f;
            display_jd9853_qspi_set_vci(display_h, vci);
            FURI_LOG_I(TAG, "Display VCI set to %.2f V", vci);

        } else if(event->key == InputKeyDown) {
            display_jd9853_qspi_set_brightness(display_h, display_jd9853_qspi_get_brightness(display_h) - 2);
        } else if(event->key == InputKeyLeft) {
            float vci = display_jd9853_qspi_get_vci(display_h);
            vci -= 0.1f * mod;
            if(vci < 2.0f) vci = 2.0f;
            if(vci > 3.3f) vci = 3.3f;
            display_jd9853_qspi_set_vci(display_h, vci);
            FURI_LOG_I(TAG, "Display VCI set to %.2f V", vci);
        } else if(event->key == InputKeyOk) {
            eco = !eco;
            display_jd9853_qspi_eco_mode(display_h, eco);
        }

        if(event->key == InputKey1) {
            display_jd9853_load_config((DisplayJd9853QSPI*)display_h, jd9853_init_seq_2025_04_01_normal_white_no_reset);
            FURI_LOG_I(TAG, "Display set to white mode");
        } else if(event->key == InputKey5) {
            display_jd9853_load_config((DisplayJd9853QSPI*)display_h, jd9853_init_seq_2025_04_01_normal_white_mod_no_reset);
            FURI_LOG_I(TAG, "Display set to white mode");
        } else if(event->key == InputKey2) {
            if(mod == 1)
                mod = -1;
            else
                mod = 1;
        }
    }
}

#define tag "TestPerefSrv"

int32_t test_peref_srv(void* p) {
    UNUSED(p);

    furi_log_set_level(FuriLogLevelDebug);
    FURI_LOG_T("tag", "Trace");
    FURI_LOG_D("tag", "Debug");
    FURI_LOG_I("tag", "Info");
    FURI_LOG_W("tag", "Warning");
    FURI_LOG_E("tag", "Error");

    uint8_t duty = 0;

    // GpioPin* ws2812_pins = (GpioPin*)malloc(sizeof(GpioPin) * 3);
    // ws2812_pins[0] = gpio_status_led_line1;
    // ws2812_pins[1] = gpio_status_led_line2;
    // ws2812_pins[2] = gpio_status_led_line3;
    // Ws2812* ws2812 = ws2812_init(ws2812_pins, 3);
    // free(ws2812_pins);

    // //furi_hal_i2c_bus_scan_print(&furi_hal_i2c_handle_internal);
    // //furi_delay_ms(1000);
    // uint8_t index_led[3] = {0};

    //Ina219* ina219 = ina219_init(&furi_hal_i2c_handle_internal, INA219_ADDRESS, 0.1f, 0.4f); // 0.1 Ohm shunt, 2A max

    //display_h = furi_record_open(RECORD_DISPLAY);
    display_h = display_jd9853_qspi_init();
    display_jd9853_qspi_set_brightness(display_h, 20);
    FuriPubSub* input = furi_record_open(RECORD_INPUT_EVENTS);
    FuriPubSubSubscription* input_subscription = furi_pubsub_subscribe(input, input_events_callback, NULL);

    StatusLights* status_lights = furi_record_open(RECORD_STATUS_LIGHTS);

    while(true) {
        // display_set_brightness(display_h, 10);
        // furi_delay_ms(5000);
        // display_set_brightness(display_h, 0);
        // furi_delay_ms(5000);

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

        // uint8_t* buf = (uint8_t*)malloc( JD9853_WIDTH * JD9853_HEIGHT);
        // display_jd9853_qspi_write_buffer((DisplayJd9853QSPI*)display_h, buf, sizeof(buf)); // Update display with current buffer

        // free(buf);

        const size_t width = JD9853_WIDTH; // 1 byte per pixel
        const size_t height = JD9853_HEIGHT;
        uint8_t ton[18] = {0xFC, 0x0, 0x11, 0x22, 0x32, 0x43, 0x54, 0x65, 0x76, 0x86, 0x97, 0xA8, 0xB9, 0xCA, 0xDA, 0xEB, 0xFC, 0x0};
        uint8_t ton8[10] = {0xFC, 0x0, 36, 72, 108, 144, 180, 216, 252, 0x0};
        uint8_t* data = (uint8_t*)malloc(width * height);
        uint8_t val = 1;
        for(size_t w = 0; w < width; w += 1) {
            for(size_t h = 0; h < height - 40; h += 1) {
                data[w + h * width] = ton[val];
            }
            if(w % 16 == 10) {
                val++;
            }
        }
        val = 16;
        for(size_t w = 0; w < width; w += 1) {
            for(size_t h = 40; h < height - 70; h += 1) {
                data[w + h * width] = ton[val];
            }
            if(w % 16 == 10) {
                val--;
            }
        }

        val = 1;
        for(size_t w = 0; w < width; w += 1) {
            for(size_t h = 70; h < height - 35; h += 1) {
                data[w + h * width] = ton8[val];
            }
            if(w % 32 == 26) {
                val += 1;
            }
        }

        val = 8;
        for(size_t w = 0; w < width; w += 1) {
            for(size_t h = 70 + 35; h < height; h += 1) {
                data[w + h * width] = ton8[val];
            }
            if(w % 32 == 26) {
                val -= 1;
            }
        }

        for(size_t i = (width) * (height - 10); i < width * height; i += 1) {
            data[i] = 0x0;
        }

        for(size_t i = 0; i < (width * 10); i += 1) {
            data[i] = 0xff;
        }
        display_jd9853_qspi_write_buffer((DisplayJd9853QSPI*)display_h, data, width * height);
        free(data);

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
        furi_delay_ms(500);

        StatusLightsColor color = {.r = 127, .g = 30, .b = 30};

        status_lights_notification(status_lights, StatusLightsTypeNet, color);
        status_lights_notification(status_lights, StatusLightsTypePower, (StatusLightsColor){.r = 0, .g = 255, .b = 0});
        status_lights_notification(status_lights, StatusLightsTypeBatteryOutline, (StatusLightsColor){.r = 0, .g = 0, .b = 255});
        status_lights_notification(status_lights, StatusLightsTypeUsbWatt4, color);

        furi_delay_ms(500);
        status_lights_notification(status_lights, StatusLightsTypeNet, (StatusLightsColor){.r = 0, .g = 0, .b = 0});
        furi_delay_ms(100);
        status_lights_notification(status_lights, StatusLightsTypePower, (StatusLightsColor){.r = 0, .g = 0, .b = 0});
        furi_delay_ms(100);
        status_lights_notification(status_lights, StatusLightsTypeBatteryOutline, (StatusLightsColor){.r = 0, .g = 0, .b = 0});
        furi_delay_ms(100);
        status_lights_notification(status_lights, StatusLightsTypeUsbWatt4, (StatusLightsColor){.r = 0, .g = 0, .b = 0});

        furi_delay_ms(500);

        status_lights_notification(status_lights, StatusLightsTypeNet, color);
        status_lights_notification(status_lights, StatusLightsTypeBatteryWatt1, (StatusLightsColor){.r = 0, .g = 255, .b = 0});
        status_lights_notification(status_lights, StatusLightsTypeBatteryOutline, (StatusLightsColor){.r = 0, .g = 0, .b = 255});
        status_lights_notification(status_lights, StatusLightsTypeUsbWatt3, color);
        furi_delay_ms(500);
        status_lights_notification(status_lights, StatusLightsTypeLine1Off, (StatusLightsColor){.r = 0, .g = 0, .b = 0});
        furi_delay_ms(500);
        status_lights_notification(status_lights, StatusLightsTypeLine2Off, (StatusLightsColor){.r = 0, .g = 0, .b = 0});
        furi_delay_ms(500);
        status_lights_notification(status_lights, StatusLightsTypeLine3Off, (StatusLightsColor){.r = 0, .g = 0, .b = 0});
        furi_delay_ms(500);

        status_lights_notification(status_lights, StatusLightsTypeEth2, color);
        status_lights_notification(status_lights, StatusLightsTypeBatteryWatt4, (StatusLightsColor){.r = 0, .g = 255, .b = 0});
        status_lights_notification(status_lights, StatusLightsTypeBatteryWatt1, (StatusLightsColor){.r = 0, .g = 0, .b = 255});
        status_lights_notification(status_lights, StatusLightsTypeUsbWatt2, color);
        furi_delay_ms(500);
        status_lights_notification(status_lights, StatusLightsTypeLineAllOff, (StatusLightsColor){.r = 0, .g = 0, .b = 0});
        furi_delay_ms(500);

        //test line 1
        // uint32_t line_buffer_1[4];
        // for(size_t i = 0; i < sizeof(line_buffer_1) / 4; i++) {
        //     if(index_led[0] == i) {
        //         line_buffer_1[i] = ws2812_urgb_u32(127, 30, 30);
        //     } else {
        //         line_buffer_1[i] = ws2812_urgb_u32(0, 0, 0);
        //     }
        // }

        // ws2812_write_buffer_dma(ws2812, 0, line_buffer_1, 4);
        // index_led[0]++;
        // if(index_led[0] >= sizeof(line_buffer_1) / 4) {
        //     index_led[0] = 0;
        // }

        // //test line 2
        // uint32_t line_buffer_2[7];
        // for(size_t i = 0; i < sizeof(line_buffer_2) / 4; i++) {
        //     if(index_led[1] == i) {
        //         line_buffer_2[i] = ws2812_urgb_u32(127, 30, 30);
        //     } else {
        //         line_buffer_2[i] = ws2812_urgb_u32(0, 0, 0);
        //     }
        // }

        // ws2812_write_buffer_dma(ws2812, 1, line_buffer_2, 7);
        // index_led[1]++;
        // if(index_led[1] >= sizeof(line_buffer_2) / 4) {
        //     index_led[1] = 0;
        // }

        // //test line 3
        // uint32_t line_buffer_3[6];
        // for(size_t i = 0; i < sizeof(line_buffer_3) / 4; i++) {
        //     if(index_led[2] == i) {
        //         line_buffer_3[i] = ws2812_urgb_u32(127, 30, 30);
        //     } else {
        //         line_buffer_3[i] = ws2812_urgb_u32(0, 0, 0);
        //     }
        // }

        // ws2812_write_buffer_dma(ws2812, 2, line_buffer_3, 6);
        // index_led[2]++;
        // if(index_led[2] >= sizeof(line_buffer_3) / 4) {
        //     index_led[2] = 0;
        // }

        //furi_hal_i2c_acquire(&furi_hal_i2c_handle_internal);
        // furi_hal_i2c_bus_scan_print(&furi_hal_i2c_handle_internal);
        // furi_hal_i2c_release(&furi_hal_i2c_handle_internal);
        // uint16_t input_state = tca6416a_read_input(tca6416a);
        // FURI_LOG_I(tag, "TCA6416A input state bin: %016b", input_state);
        // furi_thread_yield();
    }
    furi_crash();
}
