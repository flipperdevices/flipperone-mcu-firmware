#include "led_patterns.h"

#include <toolbox/color.h>

#define LED_PATTERN_GRADIENT_VALUE  (128U)
#define LED_PATTERN_FADE_HALF_STEPS (128U)

static LedColor led_pattern_static_color(LedColor base_color, uint8_t* phase) {
    UNUSED(phase);
    return base_color;
}

static LedColor led_pattern_rainbow_gradient(LedColor base_color, uint8_t* phase) {
    UNUSED(base_color);

    (*phase)++;
    const ColorHsv hsv = {
        .h = *phase,
        .s = UINT8_MAX,
        .v = LED_PATTERN_GRADIENT_VALUE,
    };
    const Color color = color_hsv_to_rgb(hsv);

    return (LedColor){.r = color.r, .g = color.g, .b = color.b};
}

static LedColor led_color_scale(LedColor color, uint8_t level) {
    return (LedColor){
        .r = (uint8_t)(((uint16_t)color.r * level) / UINT8_MAX),
        .g = (uint8_t)(((uint16_t)color.g * level) / UINT8_MAX),
        .b = (uint8_t)(((uint16_t)color.b * level) / UINT8_MAX),
    };
}

static LedColor led_pattern_fade(LedColor base_color, uint8_t* phase) {
    const uint8_t step = (*phase)++;
    // triangle-shaped brightness modulation: 1 -> 255 -> 0
    const uint16_t level = (step < LED_PATTERN_FADE_HALF_STEPS) ? (uint16_t)(step * 2U + 1U) : (uint16_t)((UINT8_MAX - step) * 2U);

    return led_color_scale(base_color, (uint8_t)level);
}

static LedColor led_pattern_blink(LedColor base_color, uint8_t* phase) {
    const bool is_on = ((*phase)++ % 2) == 0;

    return is_on ? base_color : LED_COLOR_BLACK;
}

const LedPatternPreset led_pattern_presets[LedPatternMax] = {
    [LedPatternStaticColor] =
        {
            .callback = led_pattern_static_color,
            .period_ms = 1000,
        },
    [LedPatternRainbowGradient] =
        {
            .callback = led_pattern_rainbow_gradient,
            .period_ms = 16,
        },
    [LedPatternFade] =
        {
            .callback = led_pattern_fade,
            .period_ms = 10,
        },
    [LedPatternBlink] =
        {
            .callback = led_pattern_blink,
            .period_ms = 500,
        },
};
