#include "display_jd9853_spi.h"
#include "display_jd9853_reg.h"

#include <furi.h>
#include <furi_hal_gpio.h>
#include <furi_hal_resources.h>
#include <furi_hal_pwm.h>
#include <furi_hal_spi.h>
#include <furi_hal_spi_types_i.h>

#define DISPLAY_JD9853_BAUDRATE          (32 * 1000 * 1000)
#define DISPLAY_JD9853_BACKLIGHT_BIT     8 //8-bit PWM for backlight
#define DISPLAY_JD9853_BACKLIGHT_FREQ_HZ 40000 //25kHz PWM for backlight
#define DISPLAY_JD9853_TE_FLAG           (1 << 0)
struct DisplayJd9853SPI {
    FuriHalSpiHandle* spi_handle;

    const GpioPin* pin_dc;
    const GpioPin* pin_reset;

    FuriHalPwm* backlight_pwm;
    uint8_t backlight;
    FuriEventFlag* te_flag;
};

static FURI_ALWAYS_INLINE void display_jd9853_spi_write_reg(DisplayJd9853SPI* display, DisplayJd9853Reg reg) {
    furi_hal_gpio_write(display->pin_dc, false); // DC = 0 for command
    furi_hal_spi_tx_blocking(display->spi_handle, &reg, 1);
    furi_hal_gpio_write(display->pin_dc, true); // DC = 1 for data
}

static FURI_ALWAYS_INLINE void display_jd9853_spi_write_data(DisplayJd9853SPI* display, uint8_t* data, size_t size) {
    furi_hal_spi_tx_blocking(display->spi_handle, data, size);
}

static FURI_ALWAYS_INLINE void display_jd9853_spi_load_config(DisplayJd9853SPI* display, const uint8_t* config) {
    while(*config) {
        display_jd9853_spi_write_reg(display, (DisplayJd9853Reg)(*(config + 2)));

        if(*(config)) {
            display_jd9853_spi_write_data(display, (uint8_t*)(config + 3), *(config)-1);
        }
        furi_delay_ms(*(config + 1) * 5);
        config += *(config) + 2;
    }
}

static FURI_ALWAYS_INLINE void display_jd9853_spi_set_window(DisplayJd9853SPI* display, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t caset_data[4] = {(uint8_t)(x0 >> 8), (uint8_t)(x0 & 0xFF), (uint8_t)(x1 >> 8), (uint8_t)(x1 & 0xFF)};
    uint8_t paset_data[4] = {(uint8_t)(y0 >> 8), (uint8_t)(y0 & 0xFF), (uint8_t)(y1 >> 8), (uint8_t)(y1 & 0xFF)};

    display_jd9853_spi_write_reg(display, caset); // Column address set
    display_jd9853_spi_write_data(display, caset_data, sizeof(caset_data));

    display_jd9853_spi_write_reg(display, paset); // Page address set
    display_jd9853_spi_write_data(display, paset_data, sizeof(paset_data));
}

static void display_jd9853_spi_te_callback(void* ctx) {
    DisplayJd9853SPI* display = (DisplayJd9853SPI*)ctx;
    furi_event_flag_set(display->te_flag, DISPLAY_JD9853_TE_FLAG);
}

FURI_ALWAYS_INLINE void
    display_jd9853_spi_write_buffer_x_y(DisplayJd9853SPI* display, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t* buffer, size_t size) {
    display_jd9853_spi_set_window(display, JD9853_OFF_X0 + x, JD9853_OFF_Y0 + y, JD9853_OFF_X1 + x + (w / 3) - 1, JD9853_OFF_Y1 + y + h - 1);
    display_jd9853_spi_write_reg(display, ramwr);

    // wait for te high
    furi_hal_gpio_enable_int_callback(&gpio_display_te);
    furi_event_flag_wait(display->te_flag, DISPLAY_JD9853_TE_FLAG, FuriFlagWaitAll, FuriWaitForever);
    furi_hal_gpio_disable_int_callback(&gpio_display_te);

    display_jd9853_spi_write_data(display, (uint8_t*)buffer, size);
}

FURI_ALWAYS_INLINE void display_jd9853_spi_write_buffer(DisplayJd9853SPI* display, uint16_t w, uint16_t h, const uint8_t* buffer, size_t size) {
    display_jd9853_spi_write_buffer_x_y(display, 0, 0, w, h, buffer, size);
}

void display_jd9853_spi_fill(DisplayJd9853SPI* display, uint8_t color) {
    const size_t width = JD9853_WIDTH; // 1 byte per pixel
    const size_t height = JD9853_HEIGHT;

    uint8_t* data = (uint8_t*)malloc(width * height);
    for(size_t i = 0; i < width * height; i += 1) {
        data[i] = color;
    }

    display_jd9853_spi_write_buffer(display, width, height, data, width * height);
    free(data);
}

DisplayJd9853SPI* display_jd9853_spi_init(void) {
    DisplayJd9853SPI* display = malloc(sizeof(DisplayJd9853SPI));

    display->spi_handle = malloc(sizeof(FuriHalSpiHandle));
    display->spi_handle->id = FuriHalSpiIdSPI0;
    display->spi_handle->in_use = true;
    display->pin_dc = &gpio_display_dc;
    display->pin_reset = &gpio_display_reset;
    display->te_flag = furi_event_flag_alloc();

    furi_hal_spi_init(display->spi_handle, DISPLAY_JD9853_BAUDRATE, FuriHalSpiTransferMode0, FuriHalSpiTransferBitOrderMsbFirst, FuriHalSpiModeMaster);

    // Gpio init
    furi_hal_gpio_init_simple(display->pin_dc, GpioModeOutputPushPull);
    furi_hal_gpio_init_simple(display->pin_reset, GpioModeOutputPushPull);
    furi_hal_gpio_write(display->pin_dc, true);
    furi_hal_gpio_init_simple(&gpio_display_te, GpioModeInput);

    furi_hal_gpio_add_int_callback(&gpio_display_te, GpioConditionRise, display_jd9853_spi_te_callback, display);
    furi_hal_gpio_disable_int_callback(&gpio_display_te);

    // Reset display
    furi_hal_gpio_write(display->pin_reset, false);
    furi_delay_ms(30);
    furi_hal_gpio_write(display->pin_reset, true);
    furi_delay_ms(30);

    // Initialization sequence
    // display_jd9853_spi_load_config(display, jd9853_init_seq_2025_04_01_normal_white);
    display_jd9853_spi_load_config(display, jd9853_init_seq_2025_04_01_normal_black);

    display_jd9853_spi_backlight_set_brightness(display, 2); // Set backlight to 50%

    return display;
}

void display_jd9853_spi_deinit(DisplayJd9853SPI* display) {
    furi_check(display);
    display_jd9853_spi_load_config(display, st7789_deinit_seq);
    furi_hal_gpio_init_ex(display->pin_dc, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_init_ex(display->pin_reset, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_spi_deinit(display->spi_handle);
    free(display->spi_handle);
    free(display);
}

void display_jd9853_spi_eco_mode(DisplayJd9853SPI* display, bool enable) {
    furi_check(display);
    if(enable) {
        display_jd9853_spi_write_reg(display, idmon);
    } else {
        display_jd9853_spi_write_reg(display, idmoff);
    }
}

void display_jd9853_spi_on_sleep_enter(void) {
}

void display_jd9853_spi_on_sleep_exit(void) {
}

void display_jd9853_spi_backlight_set_brightness(DisplayJd9853SPI* display, uint8_t brightness) {
    furi_check(display);
    display->backlight = brightness;
    if(!display->backlight) {
        if(display->backlight_pwm) {
            furi_hal_pwm_set_duty_cycle(display->backlight_pwm, 0);
            furi_hal_pwm_deinit(display->backlight_pwm);
            display->backlight_pwm = NULL;
        }
    } else {
        uint32_t max_value = (1 << DISPLAY_JD9853_BACKLIGHT_BIT) - 1;
        uint32_t duty_cycle = (brightness * max_value) / 100;
        if(!display->backlight_pwm) {
            //To enable the device, the CTRL signal must be high for 500 µs.
            // The PWM signal can then be applied with a pulse width (tp)
            // greater or smaller than tON. To force the device into shutdown mode,
            // the CTRL signal must be low for at least 32 ms.
            // Requiring the CTRL pin to be low for 32 mS before the device enters
            // shutdown allows for PWM dimming frequencies as low as 100 Hz.
            // The device is enabled again when a CTRL signal is high for a period of 500 µs minimum.
            display->backlight_pwm = furi_hal_pwm_init(&gpio_display_ctrl, DISPLAY_JD9853_BACKLIGHT_BIT, DISPLAY_JD9853_BACKLIGHT_FREQ_HZ, false);
            furi_hal_pwm_set_duty_cycle(display->backlight_pwm, 140);
            furi_delay_us(2400);
        }
        furi_hal_pwm_set_duty_cycle(display->backlight_pwm, duty_cycle);
    }
}

uint8_t display_jd9853_spi_backlight_get_brightness(DisplayJd9853SPI* display) {
    furi_check(display);
    return display->backlight;
}
