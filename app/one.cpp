#include <stdio.h>
#include <drivers/log.hpp>
#include <drivers/display.hpp>
#include <drivers/ws2812_strip.hpp>
#include <drivers/keys.hpp>
#include <string.h>
#include <hardware/gpio.h>
#include <initializer_list>

#define WS2812_GPIO  2
#define WS2812_COUNT 16

#define D_PIN_SDA   7
#define D_PIN_SCL   6
#define D_PIN_CTRL  8
#define D_PIN_CS    3
#define D_PIN_WR    4
#define D_PIN_RESET 5
#define D_OFF_X     77
#define D_OFF_Y     0
#define D_WIDTH     258
#define D_HEIGHT    144

#define B_KEY1       15
#define B_KEY2       14
#define B_KEY3       15
#define B_KEY4       12
#define B_KEY5       11
#define B_KEY_SW     10
#define B_KEY_UP     9
#define B_KEY_LEFT   16
#define B_KEY_CENTER 17
#define B_KEY_RIGHT  18
#define B_KEY_DOWN   19
#define B_KEY_BACK   20

#define BAT_CHARGING_GPIO   24
#define BAT_CHARGE_ADC_GPIO 29

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

static const uint8_t sinustable[0x100] = {
    0x80, 0x7d, 0x7a, 0x77, 0x74, 0x70, 0x6d, 0x6a, 0x67, 0x64, 0x61, 0x5e, 0x5b, 0x58, 0x55, 0x52, 0x4f, 0x4d, 0x4a, 0x47, 0x44, 0x41, 0x3f, 0x3c, 0x39, 0x37,
    0x34, 0x32, 0x2f, 0x2d, 0x2b, 0x28, 0x26, 0x24, 0x22, 0x20, 0x1e, 0x1c, 0x1a, 0x18, 0x16, 0x15, 0x13, 0x11, 0x10, 0x0f, 0x0d, 0x0c, 0x0b, 0x0a, 0x08, 0x07,
    0x06, 0x06, 0x05, 0x04, 0x03, 0x03, 0x02, 0x02, 0x02, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02, 0x03, 0x03, 0x04, 0x05, 0x06, 0x06, 0x07,
    0x08, 0x0a, 0x0b, 0x0c, 0x0d, 0x0f, 0x10, 0x11, 0x13, 0x15, 0x16, 0x18, 0x1a, 0x1c, 0x1e, 0x20, 0x22, 0x24, 0x26, 0x28, 0x2b, 0x2d, 0x2f, 0x32, 0x34, 0x37,
    0x39, 0x3c, 0x3f, 0x41, 0x44, 0x47, 0x4a, 0x4d, 0x4f, 0x52, 0x55, 0x58, 0x5b, 0x5e, 0x61, 0x64, 0x67, 0x6a, 0x6d, 0x70, 0x74, 0x77, 0x7a, 0x7d, 0x80, 0x83,
    0x86, 0x89, 0x8c, 0x90, 0x93, 0x96, 0x99, 0x9c, 0x9f, 0xa2, 0xa5, 0xa8, 0xab, 0xae, 0xb1, 0xb3, 0xb6, 0xb9, 0xbc, 0xbf, 0xc1, 0xc4, 0xc7, 0xc9, 0xcc, 0xce,
    0xd1, 0xd3, 0xd5, 0xd8, 0xda, 0xdc, 0xde, 0xe0, 0xe2, 0xe4, 0xe6, 0xe8, 0xea, 0xeb, 0xed, 0xef, 0xf0, 0xf1, 0xf3, 0xf4, 0xf5, 0xf6, 0xf8, 0xf9, 0xfa, 0xfa,
    0xfb, 0xfc, 0xfd, 0xfd, 0xfe, 0xfe, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xfe, 0xfe, 0xfd, 0xfd, 0xfc, 0xfb, 0xfa, 0xfa, 0xf9, 0xf8, 0xf6,
    0xf5, 0xf4, 0xf3, 0xf1, 0xf0, 0xef, 0xed, 0xeb, 0xea, 0xe8, 0xe6, 0xe4, 0xe2, 0xe0, 0xde, 0xdc, 0xda, 0xd8, 0xd5, 0xd3, 0xd1, 0xce, 0xcc, 0xc9, 0xc7, 0xc4,
    0xc1, 0xbf, 0xbc, 0xb9, 0xb6, 0xb3, 0xb1, 0xae, 0xab, 0xa8, 0xa5, 0xa2, 0x9f, 0x9c, 0x99, 0x96, 0x93, 0x90, 0x8c, 0x89, 0x86, 0x83};

void test_plasma_draw(uint8_t* buffer) {
    uint8_t z0;
    uint8_t z;
    static uint8_t c1a, c1b;
    static uint8_t c2a, c2b;
    static uint8_t c1A, c1B;
    static uint8_t c2A, c2B;
    static size_t x, y;

    c1a = c1A;
    c1b = c1B;
    for(y = 0; y < D_HEIGHT; ++y) {
        c2a = c2A;
        c2b = c2B;
        z0 = sinustable[c1a] + sinustable[c1b];
        for(x = 0; x < D_WIDTH; ++x) {
            z = z0 + sinustable[c2a] + sinustable[c2b];
            set_pixel_color(buffer, x, y, z);
            c2a += 1; // 3;
            c2b += 2; // 7;
        }
        c1a += 1; // 4;
        c1b += 2; // 9;
    }

    c1A += rand() % 4; // 3;
    c1B -= 2; // 5;
    c2A += 3; // 2;
    c2B -= 1; // 3;
}

int main() {
    Log::init();

    WS2812Strip<WS2812_GPIO, LedType, WS2812_COUNT> strip;
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

    Display<D_PIN_CTRL, D_PIN_RESET, D_PIN_CS, D_PIN_SCL, D_PIN_SDA, D_PIN_WR, D_OFF_X, D_OFF_Y, D_WIDTH, D_HEIGHT> display;
    display.init();
    // display.backlight(0.04f);
    display.backlight(0.2f);
    // display.backlight(0.4f);
    // display.backlight(1.0f);

    const size_t buffer_size = D_WIDTH * D_HEIGHT;
    uint8_t buffer[buffer_size];

    display.eco_mode(false);

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

    int32_t x_pos = 0;
    int32_t y_pos = 0;

    while(true) {
        test_plasma_draw(buffer);
        for(size_t x = 0; x < 64; x++) {
            for(size_t y = 0; y < D_HEIGHT / 2; y++) {
                uint32_t color = x;
                set_pixel_color(buffer, x, y, color << 2);
            }
        }

        if(keys.is_need_update()) {
            keys.poll();
        }
        KeysInfo info = keys.get_keys_info();

        // for(auto key : info.pressed) {
        //     Log::info("Key pressed: %u", key);
        // }

        // for(auto key : info.released) {
        //     Log::info("Key released: %u", key);
        // }

        if(info.released.contains(BAT_CHARGING_GPIO)) {
            strip.set_rgb(LedType::USBWatt1, WS2812Colors::green);
            strip.set_rgb(LedType::USBWatt2, WS2812Colors::yellow);
            strip.set_rgb(LedType::USBWatt3, WS2812Colors::orange);
            strip.set_rgb(LedType::USBWatt4, WS2812Colors::red);
            strip.set_rgb(LedType::USBPlug, WS2812Colors::green);
            strip.set_rgb(LedType::BatteryCenter, WS2812Colors::green);
        }

        if(info.pressed.contains(BAT_CHARGING_GPIO)) {
            strip.set_rgb(LedType::USBWatt1, WS2812Colors::black);
            strip.set_rgb(LedType::USBWatt2, WS2812Colors::black);
            strip.set_rgb(LedType::USBWatt3, WS2812Colors::black);
            strip.set_rgb(LedType::USBWatt4, WS2812Colors::black);
            strip.set_rgb(LedType::USBPlug, WS2812Colors::black);
            strip.set_rgb(LedType::BatteryCenter, WS2812Colors::black);
        }

        if(info.state.contains(B_KEY_UP)) {
            y_pos = (y_pos - 1 + D_HEIGHT) % D_HEIGHT;
        }
        if(info.state.contains(B_KEY_DOWN)) {
            y_pos = (y_pos + 1) % D_HEIGHT;
        }
        if(info.state.contains(B_KEY_LEFT)) {
            x_pos = (x_pos - 1 + D_WIDTH) % D_WIDTH;
        }
        if(info.state.contains(B_KEY_RIGHT)) {
            x_pos = (x_pos + 1) % D_WIDTH;
        }

        set_pixel_color(buffer, x_pos, y_pos, 0xFF); // Set a pixel at the current position to white
        set_pixel_color(buffer, x_pos, y_pos + 1, 0xFF); // Set the pixel below to white
        set_pixel_color(buffer, x_pos + 1, y_pos, 0xFF); // Set the pixel to the right to white
        set_pixel_color(buffer, x_pos - 1, y_pos, 0xFF); // Set the pixel to the left to white
        set_pixel_color(buffer, x_pos, y_pos - 1, 0xFF); // Set the pixel above to white

        display.write_buffer(buffer);
        strip.flush();
    }
}
