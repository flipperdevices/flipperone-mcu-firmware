#include <ws2812.h>

struct WS2812Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

template <uint32_t pin, typename T, size_t length = 1>
class WS2812Strip {
public:
    WS2812Strip() {
        ws2812_init(pin);
        for(size_t i = 0; i < length; i++) {
            colors[i] = {0, 0, 0}; // Initialize all pixels to black
        }
    }

    void flush(void) {
        for(size_t i = 0; i < length; i++) {
            ws2812_put_pixel_rgb(colors[i].r, colors[i].g, colors[i].b);
        }
    }

    void set_rgb(T index, WS2812Color color) {
        if(index < length) {
            colors[(size_t)index] = color;
        }
    }

    void set_rgb_all(WS2812Color color) {
        for(size_t i = 0; i < length; i++) {
            colors[i] = color;
        }
    }

private:
    WS2812Color colors[length];
};
