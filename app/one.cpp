#include <stdio.h>
#include <drivers/log.hpp>
#include <drivers/display.hpp>
#include <ws2812.h>

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
#define D_WIDTH     258 / 3
#define D_HEIGHT    144

void set_strip_color(uint8_t r, uint8_t g, uint8_t b) {
    for(int i = 0; i < WS2812_COUNT; i++) {
        put_pixel_rgb(r, g, b);
    }
}

void memset_16(uint16_t* buffer, uint16_t value, size_t size) {
    for(size_t i = 0; i < size; i++) {
        buffer[i] = value;
    }
}

int main() {
    Log::init();

    init(WS2812_GPIO);
    set_strip_color(0, 1, 0);

    Display<D_PIN_CTRL, D_PIN_RESET, D_PIN_CS, D_PIN_SCL, D_PIN_SDA, D_PIN_WR, D_OFF_X, D_OFF_Y, D_WIDTH, D_HEIGHT> display;
    display.init();
    display.backlight(0.05f);

    const size_t buffer_size = D_WIDTH * D_HEIGHT;
    uint16_t buffer[buffer_size];

    while(true) {
        memset_16(buffer, 0x0000, buffer_size);
        display.write_buffer(buffer);
        sleep_ms(1000);
        memset_16(buffer, 0xFFFF, buffer_size);
        display.write_buffer(buffer);
        sleep_ms(1000);
    }
}
