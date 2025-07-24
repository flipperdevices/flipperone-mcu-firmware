#include <stdio.h>
#include <drivers/log.hpp>
#include <drivers/display.hpp>
#include <drivers/ws2812_strip.hpp>
#include <ws2812.h>
#include <string.h>

#define WS2812_GPIO  2
#define WS2812_COUNT 15

#define D_PIN_SDA   7
#define D_PIN_SCL   6
#define D_PIN_CTRL  8
#define D_PIN_CS    9
#define D_PIN_WR    10
#define D_PIN_RESET 11
#define D_OFF_X     77
#define D_OFF_Y     0
#define D_WIDTH     258
#define D_HEIGHT    144

typedef enum {
    Power,
    Unknown,
    WiFi,
    Lan2,
    Lan1,
} LedType;

void set_pixel_color(uint8_t* buffer, int32_t x, int32_t y, uint8_t color) {
    if(x >= D_WIDTH || y >= D_HEIGHT || x < 0 || y < 0) {
        return;
    }

    x = D_WIDTH - x - 1;
    y = D_HEIGHT - y - 1;

    const uint32_t index = y * D_WIDTH + x;
    buffer[index] = color;
}

int main() {
    Log::init();

    WS2812Strip<WS2812_GPIO, LedType, WS2812_COUNT> strip;
    strip.set_rgb_all({6, 2, 0});
    strip.set_rgb(LedType::Power, {0, 5, 0});
    strip.set_rgb(LedType::Unknown, {5, 0, 0});
    strip.set_rgb(LedType::WiFi, {5, 0, 0});
    strip.set_rgb(LedType::Lan2, {5, 0, 0});
    strip.set_rgb(LedType::Lan1, {5, 0, 0});
    strip.flush();

    Display<D_PIN_CTRL, D_PIN_RESET, D_PIN_CS, D_PIN_SCL, D_PIN_SDA, D_PIN_WR, D_OFF_X, D_OFF_Y, D_WIDTH, D_HEIGHT> display;
    display.init();
    // display.backlight(0.04f);
    display.backlight(0.4f);
    // display.backlight(1.0f);

    const size_t buffer_size = D_WIDTH * D_HEIGHT;
    uint8_t buffer_checker_8px[buffer_size];
    uint8_t buffer_checker_8px_neg[buffer_size];

    memset(buffer_checker_8px, 0x00, buffer_size);
    memset(buffer_checker_8px_neg, 0xFF, buffer_size);

    for(size_t y = 0; y < D_HEIGHT; y++) {
        for(size_t x = 0; x < D_WIDTH * 3; x++) {
            uint8_t color = ((x / 8) + (y / 8)) % 2 ? 0xFF : 0x00;
            set_pixel_color(buffer_checker_8px, x, y, color);
            set_pixel_color(buffer_checker_8px_neg, x, y, ~color & 0xFF);
        }
    }

    while(true) {
        display.write_buffer(buffer_checker_8px);
        for(size_t i = 0; i < 5; i++) {
            sleep_ms(100);
            strip.set_rgb(LedType::Lan1, {0, 0, 0});
            strip.flush();
            sleep_ms(100);
            strip.set_rgb(LedType::Lan1, {5, 5, 0});
            strip.flush();
        }

        display.write_buffer(buffer_checker_8px_neg);
        for(size_t i = 0; i < 5; i++) {
            sleep_ms(100);
            strip.set_rgb(LedType::Lan1, {0, 0, 0});
            strip.flush();
            sleep_ms(100);
            strip.set_rgb(LedType::Lan1, {5, 5, 0});
            strip.flush();
        }
    }
}
