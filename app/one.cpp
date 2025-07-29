#include <stdio.h>
#include <drivers/log.hpp>
#include <drivers/display.hpp>
#include <drivers/ws2812_strip.hpp>
#include <drivers/keys.hpp>
#include <string.h>
#include <hardware/gpio.h>
#include <hardware/adc.h>
#include <initializer_list>

#include "FreeRTOS.h"
#include "task.h"

#define WS2812_GPIO  2
#define WS2812_COUNT 16

#define D_PIN_SDA   7
#define D_PIN_SCL   6
#define D_PIN_CTRL  8
#define D_PIN_CS    3
#define D_PIN_WR    4
#define D_PIN_RESET 5
#define D_WIDTH     258
#define D_HEIGHT    144
#define D_OFF_X     77
#define D_OFF_Y     (320 - D_HEIGHT) // was 0 without mirroring and rotation

#define B_KEY1       15
#define B_KEY2       14
#define B_KEY3       13
#define B_KEY4       12
#define B_KEY5       11
#define B_KEY_SW     10
#define B_KEY_UP     9
#define B_KEY_LEFT   16
#define B_KEY_CENTER 17
#define B_KEY_RIGHT  18
#define B_KEY_DOWN   19
#define B_KEY_BACK   20

#define BAT_CHARGING_GPIO       24
#define BAT_CHARGE_ADC_GPIO     29
#define PICO_FIRST_ADC_PIN      26
#define PICO_POWER_SAMPLE_COUNT 300

typedef enum {
    Power,
    Unknown,
    WiFi,
    Lan2,
    Lan1,
    USBPlug,
    USBWatt1,
    USBWatt2,
    USBWatt3,
    USBWatt4,
    BatteryCenter,
    BatteryOutline,
    BatteryWatt1,
    BatteryWatt2,
    BatteryWatt3,
    BatteryWatt4,
    Max,
} LedType;

static_assert(Max == WS2812_COUNT, "WS2812 strip count does not match LedType enum");

void set_pixel_color(uint8_t* buffer, int32_t x, int32_t y, uint8_t color) {
    if(x >= D_WIDTH || y >= D_HEIGHT || x < 0 || y < 0) {
        return;
    }

    x = D_WIDTH - x - 1;
    y = D_HEIGHT - y - 1;

    const uint32_t index = y * D_WIDTH + x;
    buffer[index] = color;
}

WS2812Strip<WS2812_GPIO, LedType, WS2812_COUNT> strip;
Display<D_PIN_CTRL, D_PIN_RESET, D_PIN_CS, D_PIN_SCL, D_PIN_SDA, D_PIN_WR, D_OFF_X, D_OFF_Y, D_WIDTH, D_HEIGHT> hw_display;
volatile bool charging = false;
float battery_percentage = 100.0f;

static void task_charge(void* arg) {
    Log::info("Starting charge task...");

    adc_init();
    adc_gpio_init(BAT_CHARGE_ADC_GPIO);
    adc_select_input(BAT_CHARGE_ADC_GPIO - PICO_FIRST_ADC_PIN);

    adc_fifo_setup(true, false, 0, false, false);
    adc_run(true);

    // We seem to read low values initially - this seems to fix it
    int ignore_count = PICO_POWER_SAMPLE_COUNT;
    while(!adc_fifo_is_empty() || ignore_count-- > 0) {
        (void)adc_fifo_get_blocking();
    }

    while(true) {
        // read vsys
        float vsys = 0.0f;
        for(int i = 0; i < PICO_POWER_SAMPLE_COUNT; i++) {
            uint16_t val = adc_fifo_get_blocking();
            vsys += val;
        }
        vsys /= (PICO_POWER_SAMPLE_COUNT);

        const float conversion_factor = 9.0f * 3.3f / (1 << 12); // 3.3V reference, 12-bit ADC
        const float battery = vsys * conversion_factor;
        const float min_battery_voltage = 3.0f;
        const float max_battery_voltage = 4.2f;
        const float battery_range = max_battery_voltage - min_battery_voltage;
        battery_percentage = (battery - min_battery_voltage) / battery_range;
        battery_percentage *= 100.0f;

        if(battery_percentage > 100.0f) {
            battery_percentage = 100.0f; // Cap at 100%
        } else if(battery_percentage < 0.0f) {
            battery_percentage = 0.0f; // Cap at 0%
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void task_charging(void* arg) {
    Log::info("Starting led task...");

    strip.init();
    strip.set_brightness(0.02f);
    strip.set_rgb_all({0, 0, 0});
    strip.set_rgb(LedType::Power, WS2812Colors::green);
    strip.set_rgb(LedType::Unknown, WS2812Colors::light_blue);
    strip.set_rgb(LedType::WiFi, WS2812Colors::light_blue);
    strip.set_rgb(LedType::Lan2, WS2812Colors::light_blue);
    strip.set_rgb(LedType::Lan1, WS2812Colors::light_blue);
    strip.set_rgb(LedType::BatteryOutline, WS2812Colors::green);
    strip.set_rgb(LedType::BatteryWatt1, WS2812Colors::green);
    strip.set_rgb(LedType::BatteryWatt2, WS2812Colors::yellow);
    strip.set_rgb(LedType::BatteryWatt3, WS2812Colors::orange);
    strip.set_rgb(LedType::BatteryWatt4, WS2812Colors::red);
    strip.flush();

    while(true) {
        if(charging) {
            strip.set_rgb(LedType::USBWatt1, WS2812Colors::black);
            strip.set_rgb(LedType::USBWatt2, WS2812Colors::black);
            strip.set_rgb(LedType::USBWatt3, WS2812Colors::black);
            strip.set_rgb(LedType::USBWatt4, WS2812Colors::black);
            strip.set_rgb(LedType::USBPlug, WS2812Colors::green);
            strip.set_rgb(LedType::BatteryCenter, WS2812Colors::green);
            strip.flush();
        } else {
            strip.set_rgb(LedType::USBWatt1, WS2812Colors::black);
            strip.set_rgb(LedType::USBWatt2, WS2812Colors::black);
            strip.set_rgb(LedType::USBWatt3, WS2812Colors::black);
            strip.set_rgb(LedType::USBWatt4, WS2812Colors::black);
            strip.set_rgb(LedType::USBPlug, WS2812Colors::black);
            strip.set_rgb(LedType::BatteryCenter, WS2812Colors::black);
            strip.flush();
        }

        vTaskDelay(pdMS_TO_TICKS(200));

        if(charging) {
            strip.set_rgb(LedType::USBWatt1, WS2812Colors::green);
            strip.set_rgb(LedType::USBWatt2, WS2812Colors::black);
            strip.set_rgb(LedType::USBWatt3, WS2812Colors::black);
            strip.set_rgb(LedType::USBWatt4, WS2812Colors::black);
            strip.set_rgb(LedType::USBPlug, WS2812Colors::green);
            strip.set_rgb(LedType::BatteryCenter, WS2812Colors::green);
            strip.flush();
        } else {
            strip.set_rgb(LedType::USBWatt1, WS2812Colors::black);
            strip.set_rgb(LedType::USBWatt2, WS2812Colors::black);
            strip.set_rgb(LedType::USBWatt3, WS2812Colors::black);
            strip.set_rgb(LedType::USBWatt4, WS2812Colors::black);
            strip.set_rgb(LedType::USBPlug, WS2812Colors::black);
            strip.set_rgb(LedType::BatteryCenter, WS2812Colors::black);
            strip.flush();
        }

        vTaskDelay(pdMS_TO_TICKS(200));

        if(charging) {
            strip.set_rgb(LedType::USBWatt1, WS2812Colors::green);
            strip.set_rgb(LedType::USBWatt2, WS2812Colors::yellow);
            strip.set_rgb(LedType::USBWatt3, WS2812Colors::black);
            strip.set_rgb(LedType::USBWatt4, WS2812Colors::black);
            strip.set_rgb(LedType::USBPlug, WS2812Colors::green);
            strip.set_rgb(LedType::BatteryCenter, WS2812Colors::green);
            strip.flush();
        } else {
            strip.set_rgb(LedType::USBWatt1, WS2812Colors::black);
            strip.set_rgb(LedType::USBWatt2, WS2812Colors::black);
            strip.set_rgb(LedType::USBWatt3, WS2812Colors::black);
            strip.set_rgb(LedType::USBWatt4, WS2812Colors::black);
            strip.set_rgb(LedType::USBPlug, WS2812Colors::black);
            strip.set_rgb(LedType::BatteryCenter, WS2812Colors::black);
            strip.flush();
        }

        vTaskDelay(pdMS_TO_TICKS(200));

        if(charging) {
            strip.set_rgb(LedType::USBWatt1, WS2812Colors::green);
            strip.set_rgb(LedType::USBWatt2, WS2812Colors::yellow);
            strip.set_rgb(LedType::USBWatt3, WS2812Colors::orange);
            strip.set_rgb(LedType::USBWatt4, WS2812Colors::black);
            strip.set_rgb(LedType::USBPlug, WS2812Colors::green);
            strip.set_rgb(LedType::BatteryCenter, WS2812Colors::green);
            strip.flush();
        } else {
            strip.set_rgb(LedType::USBWatt1, WS2812Colors::black);
            strip.set_rgb(LedType::USBWatt2, WS2812Colors::black);
            strip.set_rgb(LedType::USBWatt3, WS2812Colors::black);
            strip.set_rgb(LedType::USBWatt4, WS2812Colors::black);
            strip.set_rgb(LedType::USBPlug, WS2812Colors::black);
            strip.set_rgb(LedType::BatteryCenter, WS2812Colors::black);
            strip.flush();
        }

        vTaskDelay(pdMS_TO_TICKS(200));

        if(charging) {
            strip.set_rgb(LedType::USBWatt1, WS2812Colors::green);
            strip.set_rgb(LedType::USBWatt2, WS2812Colors::yellow);
            strip.set_rgb(LedType::USBWatt3, WS2812Colors::orange);
            strip.set_rgb(LedType::USBWatt4, WS2812Colors::red);
            strip.set_rgb(LedType::USBPlug, WS2812Colors::green);
            strip.set_rgb(LedType::BatteryCenter, WS2812Colors::green);
            strip.flush();
        } else {
            strip.set_rgb(LedType::USBWatt1, WS2812Colors::black);
            strip.set_rgb(LedType::USBWatt2, WS2812Colors::black);
            strip.set_rgb(LedType::USBWatt3, WS2812Colors::black);
            strip.set_rgb(LedType::USBWatt4, WS2812Colors::black);
            strip.set_rgb(LedType::USBPlug, WS2812Colors::black);
            strip.set_rgb(LedType::BatteryCenter, WS2812Colors::black);
            strip.flush();
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

#include <lvgl/lvgl.h>
#include "theme/lv_theme_one.h"
#include <hardware/timer.h>

static uint32_t lvgl_get_milliseconds_callback() {
    return (uint32_t)(time_us_32() / 1000);
}

const lv_color_format_t buffer_color_format = LV_COLOR_FORMAT_L8;
const size_t buffer_bytes_per_pixel = LV_COLOR_FORMAT_GET_SIZE(buffer_color_format);
const size_t buffer_size = D_WIDTH * D_HEIGHT * buffer_bytes_per_pixel;
uint8_t buffer[buffer_size];

void lvgl_flush_callback(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    hw_display.write_buffer(buffer);
    lv_display_flush_ready(disp);
}

static uint32_t time_get_hours() {
    uint64_t us = time_us_64();
    uint32_t minutes = (uint32_t)(us / 1000000);
    uint32_t hours = minutes / 60;
    return hours % 24;
}
static uint32_t time_get_minutes() {
    uint64_t us = time_us_64();
    uint32_t minutes = (uint32_t)(us / 1000000);
    return minutes % 60;
}

static void task_main(void* arg) {
    Log::info("Starting main task...");

    hw_display.init();
    // hw_display.backlight(0.04f);
    hw_display.backlight(0.2f);
    // hw_display.backlight(0.4f);
    // hw_display.backlight(0.9f);
    // hw_display.backlight(1.0f);

    lv_init();

    lv_tick_set_cb(lvgl_get_milliseconds_callback);
    lv_display_t* display1 = lv_display_create(D_WIDTH, D_HEIGHT);
    lv_display_set_antialiasing(display1, false);
    lv_theme_one_init(display1);
    lv_display_set_theme(display1, lv_theme_one_get());

    lv_display_set_flush_cb(display1, lvgl_flush_callback);
    lv_display_set_color_format(display1, buffer_color_format);
    lv_display_set_buffers(display1, buffer, NULL, buffer_size, LV_DISPLAY_RENDER_MODE_DIRECT);

    /*Change the active screen's background color*/
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x000000), LV_PART_MAIN);

    lv_obj_t* root = lv_obj_create(lv_screen_active());
    lv_obj_set_size(root, D_WIDTH, D_HEIGHT);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(root, lv_color_hex(0x000000), LV_PART_MAIN);

    lv_obj_t* top_bar = lv_obj_create(root);
    lv_obj_set_size(top_bar, D_WIDTH, 12);
    lv_obj_set_style_bg_color(top_bar, lv_color_hex(0xFFFFFF), LV_PART_MAIN);

    lv_obj_t* time = lv_label_create(top_bar);
    lv_label_set_text_fmt(time, "%02u:%02u", time_get_hours(), time_get_minutes());
    lv_obj_set_style_pad_top(time, 1, LV_PART_MAIN);
    lv_obj_set_style_text_color(time, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_align(time, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t* date = lv_label_create(top_bar);
    lv_label_set_text(date, "WED 33 JUL");
    lv_obj_set_style_pad_top(date, 1, LV_PART_MAIN);
    lv_obj_set_style_text_color(date, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_align(date, LV_ALIGN_RIGHT_MID, -40, 0);

    lv_obj_t* battery_text = lv_label_create(top_bar);
    lv_label_set_text_fmt(battery_text, "%.0f%%", battery_percentage);
    lv_obj_set_style_pad_top(battery_text, 1, LV_PART_MAIN);
    lv_obj_set_style_text_color(battery_text, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_align(battery_text, LV_ALIGN_RIGHT_MID, -2, 0);

    lv_obj_t* desktop = lv_obj_create(root);
    lv_obj_set_style_bg_color(desktop, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_flex_grow(desktop, 1);

    lv_obj_t* bottom_bar = lv_obj_create(root);
    lv_obj_set_size(bottom_bar, D_WIDTH, 16);
    lv_obj_set_style_bg_color(bottom_bar, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_flex_flow(bottom_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_layout(bottom_bar, LV_LAYOUT_FLEX);
    lv_obj_set_style_pad_left(bottom_bar, 1, LV_PART_MAIN);

    const char* button_texts[] = {"CANCEL", "SEARCH", "MENU", "VERBOSE", "START"};
    const size_t button_count = sizeof(button_texts) / sizeof(button_texts[0]);

    lv_obj_t* container[button_count];

    for(size_t i = 0; i < button_count; i++) {
        container[i] = lv_obj_create(bottom_bar);
        lv_obj_set_style_bg_color(container[i], lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_text_color(container[i], lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_border_color(container[i], lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_border_width(container[i], 1, LV_PART_MAIN);
        lv_obj_set_size(container[i], (D_WIDTH / button_count) - 1, 17);
        lv_obj_set_style_margin_left(container[i], 1, LV_PART_MAIN);

        lv_obj_t* label = lv_label_create(container[i]);
        lv_label_set_text(label, button_texts[i]);
        lv_obj_set_style_text_font(label, &lv_font_profont_12, LV_PART_MAIN);
        lv_obj_center(label);
    }

    // /*Create a white label, set its text and align it to the center*/
    // lv_obj_t* label = lv_label_create(lv_screen_active());
    // lv_label_set_text(label, "Need help, buddy?\nasdasd");
    // lv_obj_set_style_text_font(label, &lv_font_profont_12, LV_PART_MAIN);
    // lv_obj_set_style_text_color(label, lv_color_hex(0xffffff), LV_PART_MAIN);
    // lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    Keys keys = {
        B_KEY1,
        B_KEY2,
        B_KEY3,
        B_KEY4,
        B_KEY5,
        B_KEY_SW,
        B_KEY_UP,
        B_KEY_LEFT,
        B_KEY_CENTER,
        B_KEY_RIGHT,
        B_KEY_DOWN,
        B_KEY_BACK,
        BAT_CHARGING_GPIO,
    };

    xTaskCreate(task_charging, "task_charging", 256, NULL, configMAX_PRIORITIES - 1, NULL);
    xTaskCreate(task_charge, "task_charge", 256, NULL, configMAX_PRIORITIES - 1, NULL);

    while(true) {
        // test_plasma_draw(buffer);

        if(keys.is_need_update()) {
            keys.poll();
        }
        KeysInfo info = keys.get_keys_info();
        if(info.pressed.contains(BAT_CHARGING_GPIO)) charging = false;
        if(info.released.contains(BAT_CHARGING_GPIO)) charging = true;

        std::initializer_list<uint32_t> key_list = {B_KEY1, B_KEY2, B_KEY3, B_KEY4, B_KEY5};

        size_t i = 0;
        for(auto key : key_list) {
            if(info.pressed.contains(key)) {
                lv_obj_set_style_bg_color(container[i], lv_color_hex(0xFFFFFF), LV_PART_MAIN);
                lv_obj_set_style_text_color(container[i], lv_color_hex(0x000000), LV_PART_MAIN);
            }
            if(info.released.contains(key)) {
                lv_obj_set_style_bg_color(container[i], lv_color_hex(0x000000), LV_PART_MAIN);
                lv_obj_set_style_text_color(container[i], lv_color_hex(0xFFFFFF), LV_PART_MAIN);
            }
            i++;
        }

        lv_label_set_text_fmt(time, "%02u:%02u", time_get_hours(), time_get_minutes());

        lv_label_set_text_fmt(battery_text, "%.0f%%", battery_percentage);

        // for(auto key : info.pressed) {
        //     Log::info("Key pressed: %u", key);
        // }

        // for(auto key : info.released) {
        //     Log::info("Key released: %u", key);
        // }

        // hw_display.write_buffer(buffer);
        // vTaskDelay(pdMS_TO_TICKS(10));

        uint32_t time_till_next = lv_timer_handler();
        if(time_till_next == LV_NO_TIMER_READY) time_till_next = LV_DEF_REFR_PERIOD; /*handle LV_NO_TIMER_READY. Another option is to `sleep` for longer*/
        vTaskDelay(pdMS_TO_TICKS(time_till_next));
    }
}

int main() {
    Log::init();

    xTaskCreate(task_main, "task_main", 1024, NULL, configMAX_PRIORITIES - 1, NULL);

    vTaskStartScheduler();

    /* should never reach here */
    panic_unsupported();
}
