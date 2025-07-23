#include <stdio.h>
#include <drivers/log.hpp>
#include <ws2812.h>

#define WS2812_GPIO  2
#define WS2812_COUNT 15

void set_strip_color(uint8_t r, uint8_t g, uint8_t b) {
    for(int i = 0; i < WS2812_COUNT; i++) {
        put_pixel_rgb(r, g, b);
    }
}

int main() {
    Log::init(); // Initialize logging system

    init(WS2812_GPIO);

    Log::info("Hello, world!");

    uint8_t r = 5;
    uint8_t g = 10;
    uint8_t b = 0;

    set_strip_color(r, g, b); // Set pixel to red

    while(true) {
    }
}
