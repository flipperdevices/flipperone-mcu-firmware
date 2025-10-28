#include <stdio.h>
#include <drivers/log.hpp>
#include <drivers/display.hpp>
#include <drivers/ws2812_strip.hpp>
#include <drivers/keys.hpp>
#include <string.h>
#include <hardware/gpio.h>
#include <hardware/adc.h>
#include <initializer_list>
#include <functional>

#include "FreeRTOS.h"
#include "task.h"
#include "pico/multicore.h"

#define WS2812_GPIO  2
#define WS2812_COUNT 16

#define D_PIN_CTRL  8
#define D_PIN_SDA   7
#define D_PIN_SCL   6
#define D_PIN_RESET 5
#define D_PIN_WR    4
#define D_PIN_CS    3
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

#define GPIO_NORMAL_BLACK 21

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
// uint8_t buffer_hw[buffer_size];

void lvgl_flush_callback(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    // for(size_t i = 0; i < buffer_size; i++) {
    //     // convert from 8 bit to 6 bit
    //     buffer_hw[i] = buffer[i] / 4; // 8 bit to 6 bit conversion
    //     buffer_hw[i] = buffer_hw[i] << 2; // shift left to fill the 6 bits
    // }
    // hw_display.write_buffer(buffer_hw);

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

static void lv_rt_log(lv_log_level_t level, const char* buf) {
    switch(level) {
    case LV_LOG_LEVEL_TRACE:
        Log::trace_no_newline("%s", buf);
        break;
    case LV_LOG_LEVEL_INFO:
        Log::info_no_newline("%s", buf);
        break;
    case LV_LOG_LEVEL_WARN:
        Log::warn_no_newline("%s", buf);
        break;
    case LV_LOG_LEVEL_ERROR:
        Log::error_no_newline("%s", buf);
        break;
    case LV_LOG_LEVEL_USER:
        Log::user_no_newline("%s", buf);
        break;

    default:
        break;
    }
}

const float brightness_array[] = {0.0f, 0.04f, 0.2f, 0.4f, 0.6f, 0.8f, 1.0f};
const size_t brightness_array_size = sizeof(brightness_array) / sizeof(float);
size_t brightness_current_index = 2;

static void brightness_change() {
    brightness_current_index++;
    if(brightness_current_index >= brightness_array_size) {
        brightness_current_index = 0;
    }
    Log::info("Brightness changed to %.2f", brightness_array[brightness_current_index]);
    hw_display.backlight(brightness_array[brightness_current_index]);
}

const lv_color_t color_black = lv_color_hex(0xFFFFFF);
const lv_color_t color_white = lv_color_hex(0x000000);

#include "screensaver.h"
#include "phrase.h"

static void task_main(void* arg) {
    Log::info("Starting main task...");

    gpio_init(GPIO_NORMAL_BLACK);
    gpio_pull_up(GPIO_NORMAL_BLACK);

    gpio_init(B_KEY_BACK);
    gpio_pull_up(B_KEY_BACK);

    sleep_ms(10);
    bool normal_black = !gpio_get(GPIO_NORMAL_BLACK);
    bool eco_mode = false;

    if(!gpio_get(B_KEY_BACK)) {
        normal_black = !normal_black;
    }

    Log::info("Normal black: %s", normal_black ? "true" : "false");

    hw_display.init(normal_black);
    hw_display.eco_mode(eco_mode);
    brightness_change();

    lv_init();
    lv_log_register_print_cb(lv_rt_log);

    LV_IMG_DECLARE(statusbar256x12);
    LV_IMG_DECLARE(graph_256x104);
    LV_IMG_DECLARE(button_50x14);
    LV_IMG_DECLARE(button_pressed_50x14);

    LV_IMG_DECLARE(test_app_switch);
    LV_IMG_DECLARE(test_app);
    LV_IMG_DECLARE(test_gradient);
    LV_IMG_DECLARE(test_main_screen);

    LV_IMG_DECLARE(normal1);
    LV_IMG_DECLARE(normal2);
    LV_IMG_DECLARE(normal3);

    LV_IMG_DECLARE(invert1);
    LV_IMG_DECLARE(invert2);
    LV_IMG_DECLARE(invert3);

    LV_IMG_DECLARE(flipone_00000);

    lv_tick_set_cb(lvgl_get_milliseconds_callback);
    lv_display_t* display1 = lv_display_create(D_WIDTH, D_HEIGHT);
    lv_display_set_antialiasing(display1, false);
    lv_theme_one_init(display1);
    lv_display_set_theme(display1, lv_theme_one_get());

    lv_display_set_flush_cb(display1, lvgl_flush_callback);
    lv_display_set_color_format(display1, buffer_color_format);
    lv_display_set_buffers(display1, buffer, NULL, buffer_size, LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_obj_set_style_bg_color(lv_screen_active(), color_black, LV_PART_MAIN);

    lv_obj_t* root = lv_obj_create(lv_screen_active());
    lv_obj_set_size(root, D_WIDTH, D_HEIGHT);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(root, color_black, LV_PART_MAIN);

    lv_obj_t* top_bar = lv_obj_create(root);
    lv_obj_set_size(top_bar, D_WIDTH, 12);
    lv_obj_set_style_bg_color(top_bar, color_black, LV_PART_MAIN);

    lv_obj_t* top_bar_background = lv_image_create(top_bar);
    lv_image_set_src(top_bar_background, &statusbar256x12);
    lv_obj_align(top_bar_background, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t* time = lv_label_create(top_bar);
    lv_label_set_text_fmt(time, "%02u:%02u", time_get_hours(), time_get_minutes());
    lv_obj_set_style_pad_top(time, 1, LV_PART_MAIN);
    lv_obj_set_style_text_color(time, color_black, LV_PART_MAIN);
    lv_obj_align(time, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t* date = lv_label_create(top_bar);
    lv_label_set_text(date, "WED 33 JUL");
    lv_obj_set_style_pad_top(date, 1, LV_PART_MAIN);
    lv_obj_set_style_text_color(date, color_black, LV_PART_MAIN);
    lv_obj_align(date, LV_ALIGN_RIGHT_MID, -41, 0);

    lv_obj_t* battery_text = lv_label_create(top_bar);
    lv_label_set_text_fmt(battery_text, "%.0f%%", battery_percentage);
    lv_obj_set_style_pad_top(battery_text, 1, LV_PART_MAIN);
    lv_obj_set_style_text_color(battery_text, color_black, LV_PART_MAIN);
    lv_obj_align(battery_text, LV_ALIGN_RIGHT_MID, -2, -2);
    lv_obj_set_style_text_font(battery_text, &lv_font_tiny5_8, LV_PART_MAIN);

    lv_obj_t* battery_line = lv_obj_create(top_bar);
    lv_obj_set_size(battery_line, 2, 5);
    lv_obj_set_style_bg_color(battery_line, color_black, LV_PART_MAIN);
    lv_obj_align(battery_line, LV_ALIGN_BOTTOM_RIGHT, -24, -3);

    // Create the desktop

    lv_obj_t* desktop = lv_obj_create(root);
    lv_obj_set_size(desktop, D_WIDTH, D_HEIGHT - 12);
    lv_obj_set_flex_flow(desktop, LV_FLEX_FLOW_COLUMN);
    // lv_obj_add_flag(desktop, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* wallpaper = lv_obj_create(desktop);
    lv_obj_set_width(wallpaper, D_WIDTH);
    lv_obj_set_style_bg_color(wallpaper, color_black, LV_PART_MAIN);
    lv_obj_set_flex_grow(wallpaper, 1);

    lv_obj_t* desktop_background = lv_image_create(wallpaper);
    lv_image_set_src(desktop_background, &graph_256x104);
    lv_obj_align(desktop_background, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t* bottom_bar = lv_obj_create(desktop);
    lv_obj_set_size(bottom_bar, D_WIDTH, 14);
    lv_obj_set_style_bg_color(bottom_bar, color_black, LV_PART_MAIN);
    lv_obj_set_flex_flow(bottom_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_layout(bottom_bar, LV_LAYOUT_FLEX);
    lv_obj_set_style_pad_left(bottom_bar, 1, LV_PART_MAIN);

    const char* button_texts[] = {"CANCEL", "SEARCH", "MENU", "VERBOSE", "START"};
    const size_t button_count = sizeof(button_texts) / sizeof(button_texts[0]);

    typedef struct {
        lv_obj_t* container;
        lv_obj_t* image;
        lv_obj_t* label;
    } ButtonContainer;

    ButtonContainer buttons[button_count];

    for(size_t i = 0; i < button_count; i++) {
        buttons[i].container = lv_obj_create(bottom_bar);
        lv_obj_set_size(buttons[i].container, 50, 14);
        lv_obj_set_style_text_color(buttons[i].container, color_black, LV_PART_MAIN);

        buttons[i].image = lv_image_create(buttons[i].container);
        lv_image_set_src(buttons[i].image, &button_50x14);
        lv_obj_set_style_margin_left(buttons[i].container, 1, LV_PART_MAIN);

        buttons[i].label = lv_label_create(buttons[i].image);
        lv_label_set_text(buttons[i].label, button_texts[i]);
        lv_obj_set_style_text_font(buttons[i].label, &lv_font_profont_12, LV_PART_MAIN);
        lv_obj_center(buttons[i].label);
    }

    // {
    // lv_obj_add_flag(root, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* root_new = lv_obj_create(lv_screen_active());
    lv_obj_set_size(root_new, D_WIDTH, D_HEIGHT);
    lv_obj_set_flex_flow(root_new, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(root_new, color_black, LV_PART_MAIN);
    lv_obj_set_style_pad_left(root_new, 1, LV_PART_MAIN);

    lv_obj_t* test_image = lv_image_create(root_new);
    lv_image_set_src(test_image, &test_gradient);
    lv_obj_align(test_image, LV_ALIGN_CENTER, 0, 0);

    lv_obj_add_flag(root_new, LV_OBJ_FLAG_HIDDEN);

    // animation

    lv_obj_t* root_animation = lv_obj_create(lv_screen_active());
    lv_obj_set_size(root_animation, D_WIDTH, D_HEIGHT);
    lv_obj_set_flex_flow(root_animation, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(root_animation, color_black, LV_PART_MAIN);
    lv_obj_set_style_pad_left(root_animation, 1, LV_PART_MAIN);

    lv_obj_t* animimg = lv_animimg_create(root_animation);
    lv_obj_center(animimg);
    lv_animimg_set_src(animimg, (const void**)flipone, flipone_count);
    lv_animimg_set_duration(animimg, 1000 / 30); // 30 FPS
    lv_animimg_set_repeat_count(animimg, LV_ANIM_REPEAT_INFINITE);
    lv_animimg_start(animimg);

    // }

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

    auto roots = {
        root,
        root_new,
        root_animation,
    };

    auto screen_show = [&](lv_obj_t* obj) {
        for(auto root_obj : roots) {
            if(root_obj == obj) {
                lv_obj_clear_flag(root_obj, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(root_obj, LV_OBJ_FLAG_HIDDEN);
            }
        }
    };

    auto screen_set_picture = [&](const void* src) {
        screen_show(root_new);
        lv_image_set_src(test_image, src);
    };

    auto screen_set_animation = [&](const void** src, size_t count, size_t fps) {
        screen_show(root_animation);
        lv_animimg_set_src(animimg, src, count);
        lv_animimg_set_duration(animimg, (count * 1000) / fps);
        lv_animimg_set_repeat_count(animimg, LV_ANIM_REPEAT_INFINITE);
        lv_animimg_start(animimg);
    };

    const std::function<void()> screens_on_enter[] = {
        // [&]() { screen_set_animation((const void**)flipone, flipone_count, 30); },
        [&]() { screen_show(root); },
        [&]() { screen_set_picture(&test_gradient); },
        [&]() { screen_set_picture(&test_app); },
        [&]() { screen_set_picture(&test_app_switch); },
        [&]() { screen_set_picture(&test_main_screen); },
        [&]() { screen_set_animation((const void**)phrase, phrase_count, 10); },
        // [&]() { screen_set_picture(&normal1); },
        // [&]() { screen_set_picture(&normal2); },
        // [&]() { screen_set_picture(&normal3); },
        // [&]() { screen_set_picture(&invert1); },
        // [&]() { screen_set_picture(&invert2); },
        // [&]() { screen_set_picture(&invert3); },
        [&]() { screen_set_picture(&flipone_00000); },
    };

    const size_t screen_count = sizeof(screens_on_enter) / sizeof(screens_on_enter[0]);
    int32_t current_screen = 0;
    screens_on_enter[current_screen]();

    while(true) {
        if(keys.is_need_update()) {
            keys.poll();
        }
        KeysInfo info = keys.get_keys_info();
        if(info.pressed.contains(BAT_CHARGING_GPIO)) {
            Log::info("Battery charging stopped");
            charging = false;
        }
        if(info.released.contains(BAT_CHARGING_GPIO)) {
            Log::info("Battery charging started");
            charging = true;
        }

        const std::initializer_list<uint32_t> key_list = {B_KEY1, B_KEY2, B_KEY3, B_KEY4, B_KEY5};

        size_t i = 0;
        for(auto key : key_list) {
            if(info.pressed.contains(key)) {
                lv_image_set_src(buttons[i].image, &button_pressed_50x14);
                lv_obj_set_style_text_color(buttons[i].container, color_black, LV_PART_MAIN);
            }
            if(info.released.contains(key)) {
                lv_image_set_src(buttons[i].image, &button_50x14);
                lv_obj_set_style_text_color(buttons[i].container, color_white, LV_PART_MAIN);
            }
            i++;
        }

        if(info.pressed.contains(B_KEY1)) {
            eco_mode = !eco_mode;
            hw_display.eco_mode(eco_mode);
        }

        if(info.pressed.contains(B_KEY_SW)) {
            brightness_change();
        }

        if(info.pressed.contains(B_KEY_UP)) {
            current_screen++;
            if(current_screen >= screen_count) {
                current_screen = 0;
            }
            screens_on_enter[current_screen]();
        }

        if(info.pressed.contains(B_KEY_DOWN)) {
            current_screen--;
            if(current_screen >= screen_count) {
                current_screen = screen_count - 1;
            }
            screens_on_enter[current_screen]();
        }

        lv_label_set_text_fmt(time, "%02u:%02u", time_get_hours(), time_get_minutes());

        lv_label_set_text_fmt(battery_text, "%.0f%%", battery_percentage);
        size_t battery_height = 6 * (battery_percentage / 100.0f);
        if(battery_height > 5) {
            battery_height = 5; // maximum height
        }
        lv_obj_set_height(battery_line, battery_height);

        uint32_t time_till_next = lv_timer_handler();
        if(time_till_next == LV_NO_TIMER_READY) time_till_next = LV_DEF_REFR_PERIOD; /*handle LV_NO_TIMER_READY. Another option is to `sleep` for longer*/
        vTaskDelay(pdMS_TO_TICKS(time_till_next));
    }
}

int main() {
    Log::init();

    xTaskCreate(task_main, "task_main", 1024 * 8, NULL, configMAX_PRIORITIES - 1, NULL);

    // somehow openocd fucks up the multicore reset
    // so we need to reset core1 manually
    sleep_ms(5);
    multicore_reset_core1();
    (void)multicore_fifo_pop_blocking();

    vTaskStartScheduler();

    /* should never reach here */
    panic_unsupported();
}
