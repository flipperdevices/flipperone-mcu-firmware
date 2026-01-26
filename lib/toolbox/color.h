/**
 * @file color.h
 * @brief Color declaration and conversion API
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COLOR_MAKE_RGB(rv, gv, bv) {.b = (bv), .g = (gv), .r = (rv)}

#define COLOR_MAKE_HEX(hex) \
    {.b = (hex) & 0xFF, .g = ((hex) >> 8) & 0xFF, .r = ((hex) >> 16) & 0xFF}

/** RGB color structure */
typedef struct {
    uint8_t b; /**< Blue component */
    uint8_t g; /**< Green component */
    uint8_t r; /**< Red component */
} Color;

/** HSV color structure */
typedef struct {
    uint8_t h; /**< Hue component */
    uint8_t s; /**< Saturation component */
    uint8_t v; /**< Value component */
} ColorHsv;

/**
 * @brief Convert HSV color to RGB color
 * 
 * @param hsv ColorHsv structure to convert
 *
 * @return Color structure
 */
Color color_hsv_to_rgb(ColorHsv hsv);

/**
 * @brief Convert a HEX representation to RGB color
 *
 * @param hex Hex value to convert
 *
 * @return Color structure
 */
Color color_hex_to_rgb(uint32_t hex);

#ifdef __cplusplus
}
#endif
