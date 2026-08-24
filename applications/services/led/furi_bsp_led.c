#include "furi_bsp_led.h"

#include <furi_bsp.h>

#include <drivers/ws2812/ws2812.h>

#define LED_LINES_COUNT             (3U)
#define LED_WAIT_POWER_ON_WS2812_MS (5U)
#define LED_LINE1_INDEX             (0U)
#define LED_LINE2_INDEX             (1U)
#define LED_LINE3_INDEX             (2U)

typedef enum {
    LedUpdateLine1 = (1 << 0U),
    LedUpdateLine2 = (1 << 1U),
    LedUpdateLine3 = (1 << 2U),
} LedUpdateLine;

typedef struct {
    uint32_t line[LED_TOTAL_LEDS_COUNT];
    StatusLedPower mask_power;
    LedUpdateLine update_line;
} LedState;

struct FuriBspLed {
    Ws2812* ws2812;
    LedState led_state;
};

static LedUpdateLine led_get_update_line_by_type(LedType type) {
    switch(type) {
    // Line 1
    case LedTypeLine1Off:
    case LedTypeNet ... LedTypeEth1:
        return LedUpdateLine1;

    // Line 2
    case LedTypeLine2Off:
    case LedTypePower ... LedTypeBatteryWatt4:
        return LedUpdateLine2;

    // Line 3
    case LedTypeLine3Off:
    case LedTypeUsbCharging ... LedTypeBatteryCenter:
        return LedUpdateLine3;

    // All lines
    case LedTypeLineAllOff:
        return LedUpdateLine1 | LedUpdateLine2 | LedUpdateLine3;
    default:
        return 0;
    }
}

void furi_bsp_led_set(FuriBspLed* instance, LedType type, uint8_t r, uint8_t g, uint8_t b) {
    instance->led_state.line[type] = ws2812_urgb_u32(r, g, b);
    instance->led_state.update_line |= led_get_update_line_by_type(type);
}

void furi_bsp_led_all_off(FuriBspLed* instance) {
    memset(instance->led_state.line, 0x00, sizeof(instance->led_state.line));
}

static bool led_line_is_wanna_power(uint32_t* line_buffer, size_t led_count) {
    furi_assert(line_buffer);
    bool need_power = false;
    for(size_t i = 0; i < led_count; i++) {
        if(line_buffer[i] != 0) {
            need_power = true;
            break;
        }
    }
    return need_power;
}

static FURI_ALWAYS_INLINE bool led_start_off_timer(FuriBspLed* instance, bool check_line, StatusLedPower line_power) {
    furi_assert(instance);
    if(check_line != (instance->led_state.mask_power & line_power)) {
        if(check_line) {
            instance->led_state.mask_power |= line_power;
            furi_bsp_expander_control_led_power(instance->led_state.mask_power);
            furi_delay_ms(LED_WAIT_POWER_ON_WS2812_MS);
        } else {
            instance->led_state.mask_power &= ~line_power;
            furi_bsp_expander_control_led_power(instance->led_state.mask_power);
        }
    }
    return instance->led_state.mask_power & line_power;
}

static uint32_t* led_get_line_1(FuriBspLed* instance) {
    return instance->led_state.line;
}

static uint32_t* led_get_line_2(FuriBspLed* instance) {
    return instance->led_state.line + LED_LINE_1_LEDS_COUNT;
}

static uint32_t* led_get_line_3(FuriBspLed* instance) {
    return instance->led_state.line + LED_LINE_1_LEDS_COUNT + LED_LINE_2_LEDS_COUNT;
}

static void led_update_lines(FuriBspLed* instance, LedUpdateLine update_line) {
    furi_assert(instance);
    if(update_line & LedUpdateLine1) {
        if(led_start_off_timer(instance, led_line_is_wanna_power(led_get_line_1(instance), LED_LINE_1_LEDS_COUNT), StatusLedPowerLine1)) {
            ws2812_write_buffer_dma(instance->ws2812, LED_LINE1_INDEX, led_get_line_1(instance), LED_LINE_1_LEDS_COUNT);
        }
    }
    if(update_line & LedUpdateLine2) {
        if(led_start_off_timer(instance, led_line_is_wanna_power(led_get_line_2(instance), LED_LINE_2_LEDS_COUNT), StatusLedPowerLine2)) {
            ws2812_write_buffer_dma(instance->ws2812, LED_LINE2_INDEX, led_get_line_2(instance), LED_LINE_2_LEDS_COUNT);
        }
    }
    if(update_line & LedUpdateLine3) {
        if(led_start_off_timer(instance, led_line_is_wanna_power(led_get_line_3(instance), LED_LINE_3_LEDS_COUNT), StatusLedPowerLine3)) {
            ws2812_write_buffer_dma(instance->ws2812, LED_LINE3_INDEX, led_get_line_3(instance), LED_LINE_3_LEDS_COUNT);
        }
    }
}

void furi_bsp_led_update(FuriBspLed* instance) {
    furi_assert(instance);
    led_update_lines(instance, instance->led_state.update_line);
    instance->led_state.update_line = 0;
}

FuriBspLed* furi_bsp_led_alloc(void) {
    FuriBspLed* instance = (FuriBspLed*)malloc(sizeof(FuriBspLed));

    // Ws2812 init
    GpioPin ws2812_pins[] = {gpio_status_led_line1, gpio_status_led_line2, gpio_status_led_line3};
    instance->ws2812 = ws2812_init(ws2812_pins, LED_LINES_COUNT);

    return instance;
}

void furi_bsp_led_free(FuriBspLed* instance) {
    furi_check(instance);

    ws2812_deinit(instance->ws2812);

    free(instance);
}
