#include "furi_bsp_led.h"

#include <furi.h>
#include <furi_bsp.h>
#include <furi_hal_resources.h>
#include <furi_hal_pwm.h>

#include <drivers/ws2812/ws2812.h>

#define LED_LINES_COUNT             (3U)
#define LED_WAIT_POWER_ON_WS2812_MS (5U)
#define LED_LINE1_INDEX             (0U)
#define LED_LINE2_INDEX             (1U)
#define LED_LINE3_INDEX             (2U)

#define LED_POWER_PWM_RESOLUTION 8 // 8-bit PWM for power led
#define LED_POWER_PWM_FREQ_HZ    2000 // 2kHz PWM for power led

typedef enum {
    LedUpdateLine1 = (1 << 0U),
    LedUpdateLine2 = (1 << 1U),
    LedUpdateLine3 = (1 << 2U),
} LedUpdateLine;

typedef struct {
    uint32_t line[LED_TOTAL_LEDS_COUNT];
    StatusLedPower mask_power;
    LedUpdateLine update_line;
    FuriHalPwm* pwm_power_r;
    FuriHalPwm* pwm_power_g;
    FuriHalPwm* pwm_power_b;
} LedState;

struct FuriBspLed {
    Ws2812* ws2812;
    LedState led_state;
    const GpioPin* led_power_pin;
};

static LedUpdateLine led_get_update_line_by_type(FuriBspLedType type) {
    switch(type) {
    // Line 1
    case FuriBspLedTypeNet ... FuriBspLedTypeEth1:
        return LedUpdateLine1;

    // Line 2
    case FuriBspLedTypeBatteryOutline ... FuriBspLedTypeBatteryWatt4:
        return LedUpdateLine2;

    // Line 3
    case FuriBspLedTypeUsbCharging ... FuriBspLedTypeBatteryCenter:
        return LedUpdateLine3;

    // All lines
    case FuriBspLedTypeAllOff:
        return LedUpdateLine1 | LedUpdateLine2 | LedUpdateLine3;
    default:
        return 0;
    }
}

static uint8_t led_type_to_physical_index(FuriBspLedType type) {
    if(type > FuriBspLedTypeBatteryOutline) {
        // LedTypeBatteryOutline occupies two physical LEDs
        return (uint8_t)(type + 1);
    }
    return (uint8_t)type;
}

void furi_bsp_led_set(FuriBspLed* instance, FuriBspLedType type, uint8_t r, uint8_t g, uint8_t b) {
    furi_assert(instance);
    furi_check(type <= FuriBspLedTypeBatteryCenter);

    if(type == FuriBspLedTypePower) {
        //FURI_LOG_W("FuriBspLed", "FuriBspLedTypePower.");
        furi_hal_pwm_set_duty_cycle(instance->led_state.pwm_power_r, r);
        furi_hal_pwm_set_duty_cycle(instance->led_state.pwm_power_g, g);
        furi_hal_pwm_set_duty_cycle(instance->led_state.pwm_power_b, b);
        return;
    }

    const uint32_t color = ws2812_urgb_u32(r, g, b);
    const uint8_t index = led_type_to_physical_index(type);
    instance->led_state.line[index] = color;
    instance->led_state.update_line |= led_get_update_line_by_type(type);
    if(type == FuriBspLedTypeBatteryOutline) {
        // outline is 2 physical leds
        instance->led_state.line[index + 1] = color;
    }
}

void furi_bsp_led_all_off(FuriBspLed* instance) {
    furi_assert(instance);
    memset(instance->led_state.line, 0x00, sizeof(instance->led_state.line));
    instance->led_state.update_line |= LedUpdateLine1 | LedUpdateLine2 | LedUpdateLine3;
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

// buffer slot 4 is reserved for FuriBspLedTypePower (PWM, not in the WS2812 chain)
static uint32_t* led_get_line_2(FuriBspLed* instance) {
    // buffer slot 4 is reserved for FuriBspLedTypePower (PWM, not in the WS2812 chain)
    return instance->led_state.line + LED_LINE_1_LEDS_COUNT + 1U;
}

// buffer slot 4 is reserved for FuriBspLedTypePower (PWM, not in the WS2812 chain)
static uint32_t* led_get_line_3(FuriBspLed* instance) {
    return instance->led_state.line + LED_LINE_1_LEDS_COUNT + 1U + LED_LINE_2_LEDS_COUNT;
}

static void led_update_lines(FuriBspLed* instance, LedUpdateLine update_line) {
    furi_assert(instance);

    if(update_line) {
        furi_hal_gpio_write(instance->led_power_pin, true);
    }

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

    if(instance->led_state.mask_power == 0) {
        furi_hal_gpio_write(instance->led_power_pin, false);
    }
}

void furi_bsp_led_update(FuriBspLed* instance) {
    furi_assert(instance);
    led_update_lines(instance, instance->led_state.update_line);
    instance->led_state.update_line = 0;
}

FuriBspLed* furi_bsp_led_alloc(void) {
    FuriBspLed* instance = (FuriBspLed*)malloc(sizeof(FuriBspLed));

    // Initialize the power pin for the LEDs
    instance->led_power_pin = &gpio_status_led_dcdc_enable;
    furi_hal_gpio_init_simple(instance->led_power_pin, GpioModeOutputPushPull);
    furi_hal_gpio_write(instance->led_power_pin, false);

    // Initialize PWM for FuriBspLedTypePower
    instance->led_state.pwm_power_r = furi_hal_pwm_init(&gpio_power_led_red, LED_POWER_PWM_RESOLUTION, LED_POWER_PWM_FREQ_HZ, false);
    instance->led_state.pwm_power_g = furi_hal_pwm_init(&gpio_power_led_green, LED_POWER_PWM_RESOLUTION, LED_POWER_PWM_FREQ_HZ, true);
    instance->led_state.pwm_power_b = furi_hal_pwm_init(&gpio_power_led_blue_dfu, LED_POWER_PWM_RESOLUTION, LED_POWER_PWM_FREQ_HZ, true);
    furi_hal_pwm_set_duty_cycle(instance->led_state.pwm_power_r, 0);
    furi_hal_pwm_set_duty_cycle(instance->led_state.pwm_power_g, 0);
    furi_hal_pwm_set_duty_cycle(instance->led_state.pwm_power_b, 0);

    // Ws2812 init
    GpioPin ws2812_pins[] = {gpio_status_led_line1, gpio_status_led_line2, gpio_status_led_line3};
    instance->ws2812 = ws2812_init(ws2812_pins, LED_LINES_COUNT);

    return instance;
}

void furi_bsp_led_free(FuriBspLed* instance) {
    furi_check(instance);

    ws2812_deinit(instance->ws2812);
    furi_hal_pwm_deinit(instance->led_state.pwm_power_r);
    furi_hal_pwm_deinit(instance->led_state.pwm_power_g);
    furi_hal_pwm_deinit(instance->led_state.pwm_power_b);

    free(instance);
}
