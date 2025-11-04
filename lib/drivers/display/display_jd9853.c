#include "display_jd9853.h"
#include "display_jd9853_reg.h"

#include <furi_hal_gpio.h>
#include <furi_hal_resources.h>
#include <furi_hal_spi.h>
#include <furi_hal_spi_types_i.h>

#define DISPLAY_BAUNDRATE (100 * 1000 * 1000)

struct DisplayJd9853 {
    FuriHalSpiHandle* spi_handle;
    const GpioPin* pin_dc;
    const GpioPin* pin_reset;
};

static FURI_ALWAYS_INLINE void display_jd9853_write_reg(DisplayJd9853* display, DisplayJd9853Reg reg) {
    furi_hal_gpio_write(display->pin_dc, false); // DC = 0 for command
    furi_hal_spi_tx_blocking(display->spi_handle, &reg, 1);
    furi_hal_gpio_write(display->pin_dc, true); // DC = 1 for data
}

static FURI_ALWAYS_INLINE void display_jd9853_write_data(DisplayJd9853* display, uint8_t* data, size_t size) {
    furi_hal_spi_tx_blocking(display->spi_handle, data, size);
}

static FURI_ALWAYS_INLINE void display_jd9853_load_config(DisplayJd9853* display, const uint8_t* config) {
    while(*config) {
        display_jd9853_write_reg(display, (DisplayJd9853Reg)(*(config + 2)));
        if(*(config + 1)) {
            display_jd9853_write_data(display, (uint8_t*)(config + 3), *(config + 1));
        }
        furi_delay_ms(*(config + 1) * 5);
        config += *(config) + 2;
    }
}

static FURI_ALWAYS_INLINE void display_jd9853_set_window(DisplayJd9853* display, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t caset_data[4] = {(uint8_t)(x0 >> 8), (uint8_t)(x0 & 0xFF), (uint8_t)(x1 >> 8), (uint8_t)(x1 & 0xFF)};
    uint8_t paset_data[4] = {(uint8_t)(y0 >> 8), (uint8_t)(y0 & 0xFF), (uint8_t)(y1 >> 8), (uint8_t)(y1 & 0xFF)};

    display_jd9853_write_reg(display, caset); // Column address set
    display_jd9853_write_data(display, caset_data, sizeof(caset_data));

    display_jd9853_write_reg(display, paset); // Page address set
    display_jd9853_write_data(display, paset_data, sizeof(paset_data));
}

void display_jd9853_write_buffer(DisplayJd9853* display, uint16_t w, uint16_t h, const uint8_t* buffer, size_t size) {
    display_jd9853_set_window(display, 0, 0, (w / 3) - 1, h - 1);
    display_jd9853_write_reg(display, ramwr);
    display_jd9853_write_data(display, (uint8_t*)buffer, size);
}

void display_jd9853_fill(DisplayJd9853* display, uint8_t r, uint8_t g, uint8_t b) {
    const size_t width = SCREEN_WIDTH * 2; // 2 bytes per pixel
    const size_t height = SCREEN_HEIGHT;

    uint8_t* data = (uint8_t*)malloc(width * height);
    for(size_t i = 0; i < width * height; i += 2) {
        uint rgb = (r << 11) + (g << 5) + b;
        data[i] = rgb >> 8; // High byte
        data[i + 1] = rgb & 0xFF; // Low byte
    }

    display_jd9853_set_window(display, 0, 0, (width)-1, height - 1);
    display_jd9853_write_reg(display, ramwr);
    display_jd9853_write_data(display, data, width * height);
    free(data);
}

DisplayJd9853* display_jd9853_init(void) {
    DisplayJd9853* display = malloc(sizeof(DisplayJd9853));

    display->spi_handle = malloc(sizeof(FuriHalSpiHandle));
    display->spi_handle->id = FuriHalSpiIdSPI0;
    display->spi_handle->in_use = true;
    display->pin_dc = &gpio_display_dc;
    display->pin_reset = &gpio_display_reset;

    furi_hal_spi_init(display->spi_handle, DISPLAY_BAUNDRATE, FuriHalSpiTransferMode0, FuriHalSpiTransferBitOrderMsbFirst, FuriHalSpiModeMaster);

    //Gpio init
    furi_hal_gpio_init_simple(display->pin_dc, GpioModeOutputPushPull);
    furi_hal_gpio_init_simple(display->pin_reset, GpioModeOutputPushPull);
    furi_hal_gpio_write(display->pin_dc, false);

    //Reset display
    furi_hal_gpio_write(display->pin_reset, false);
    furi_delay_ms(30);
    furi_hal_gpio_write(display->pin_reset, true);
    furi_delay_ms(30);

    //Initialization sequence
    display_jd9853_load_config(display, st7789_init_seq);

    return display;
}

void display_jd9853_deinit(DisplayJd9853* display) {
    furi_check(display);
    display_jd9853_load_config(display, st7789_deinit_seq);
    furi_hal_gpio_init_ex(display->pin_dc, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_init_ex(display->pin_reset, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_spi_deinit(display->spi_handle);
    free(display->spi_handle);
    free(display);
}
