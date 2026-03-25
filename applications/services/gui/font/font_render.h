/*
 * u8g2_font_render.h
 *
 *  Created on: Nov 27, 2020
 *      Author: quard
 */

#ifndef INC_U8G2_FONT_RENDER_H_
#define INC_U8G2_FONT_RENDER_H_

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define U8G2_FONT_HEADER_SIZE 23

#define U8G2FontRender_OK  0x01
#define U8G2FontRender_ERR 0x02

typedef void (*U8G2FontRenderDrawPixelCallback)(int32_t x, int32_t y, void* context);

typedef struct {
    uint8_t number_of_glyphs     : 8;
    uint8_t bounding_box_mode    : 8;
    uint8_t zero_bit_width       : 8;
    uint8_t one_bit_width        : 8;

    uint8_t glyph_width          : 8;
    uint8_t glyph_height         : 8;
    uint8_t glyph_x_offset       : 8;
    uint8_t glyph_y_offset       : 8;
    uint8_t glyph_pitch          : 8;

    int8_t bounding_box_width    : 8;
    int8_t bounding_box_height   : 8;
    int8_t bounding_box_x_offset : 8;
    int8_t bounding_box_y_offset : 8;

    int8_t ascent_A              : 8;
    int8_t descent_g             : 8;
    int8_t ascent_parentheses    : 8;
    int8_t descent_parentheses   : 8;

    uint16_t offset_A            : 16;
    uint16_t offset_a            : 16;
    uint16_t offset_0x100        : 16;
} U8G2FontHeader;

typedef struct {
    uint8_t character   : 8;
    uint8_t next_glypth : 8;

    uint8_t width;
    uint8_t height;
    int8_t x_offset;
    int8_t y_offset;
    int8_t pitch;

    uint8_t bit_pos;
    const uint8_t* data;
} U8G2FontGlyph;

typedef struct {
    U8G2FontHeader header;

    const uint8_t* data;

    U8G2FontRenderDrawPixelCallback draw_pixel_fg;
    U8G2FontRenderDrawPixelCallback draw_pixel_bg;
} U8G2FontRender;

U8G2FontRender u8g2_font_render_init(const uint8_t* data, U8G2FontRenderDrawPixelCallback draw_pixel_fg, U8G2FontRenderDrawPixelCallback draw_pixel_bg);
U8G2FontHeader u8g2_font_render_parse_header(U8G2FontRender* font);
void u8g2_font_render_print_char(U8G2FontRender* font, int32_t* x, int32_t y, char chr, void* context);
void u8g2_font_render_print(U8G2FontRender* font, int32_t x, int32_t y, const char* str, void* context);

void u8g2_font_render_print_slice(U8G2FontRender* font, int32_t x, int32_t y, const char* str, size_t len, void* context);
size_t u8g2_font_render_get_height(U8G2FontRender* font);
size_t u8g2_font_render_get_string_width(U8G2FontRender* font, const char* str, size_t len);

void u8g2_font_render_print_multiline(U8G2FontRender* font, int32_t x, int32_t y, const char* str, size_t len, void* context);
size_t u8g2_font_render_get_string_width_multiline(U8G2FontRender* font, const char* str, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* INC_U8G2_FONT_RENDER_H_ */
