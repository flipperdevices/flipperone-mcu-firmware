/*
 * u8g2_font_render.c
 *
 *  Created on: Nov 27, 2020
 *      Author: quard
 */
#include "font_render.h"

uint8_t font_get_unsigned_bits(U8G2FontGlyph* glyph, uint8_t count);
int8_t font_get_signed_bits(U8G2FontGlyph* glyph, uint8_t count);
uint16_t font_get_start_symbol_search_postition(U8G2FontRender* font, char chr);
int8_t font_get_glyph(U8G2FontRender* font, U8G2FontGlyph* glyph, uint16_t search_position, char chr);
void font_parse_glyph_header(U8G2FontRender* font, U8G2FontGlyph* glyph);
int32_t font_draw_start_x_position(U8G2FontRender* font, U8G2FontGlyph* glyph);
int32_t font_draw_start_y_position(U8G2FontRender* font, U8G2FontGlyph* glyph);
void font_render_glyph(U8G2FontRender* font, U8G2FontGlyph* glyph, int32_t x, int32_t y, void* context);

U8G2FontRender u8g2_font_render_init(const uint8_t* data, U8G2FontRenderDrawPixelCallback draw_pixel_fg, U8G2FontRenderDrawPixelCallback draw_pixel_bg) {
    U8G2FontRender font = {
        .data = data,
        .draw_pixel_fg = draw_pixel_fg,
        .draw_pixel_bg = draw_pixel_bg,
    };

    font.header = u8g2_font_render_parse_header(&font);

    return font;
}

U8G2FontHeader u8g2_font_render_parse_header(U8G2FontRender* font) {
    U8G2FontHeader header;

    memcpy(&header, font->data, U8G2_FONT_HEADER_SIZE);
    header.offset_A = U8G2_FONT_HEADER_SIZE + (font->data[17] << 8 | font->data[18]);
    header.offset_a = U8G2_FONT_HEADER_SIZE + (font->data[19] << 8 | font->data[20]);
    header.offset_0x100 = U8G2_FONT_HEADER_SIZE + (font->data[21] << 8 | font->data[22]);

    return header;
}

void u8g2_font_render_print_char(U8G2FontRender* font, int32_t* x, int32_t y, char chr, void* context) {
    uint16_t search_position = font_get_start_symbol_search_postition(font, chr);

    U8G2FontGlyph glyph;
    if(font_get_glyph(font, &glyph, search_position, chr) != U8G2FontRender_OK) {
        return;
    }
    font_parse_glyph_header(font, &glyph);
    if(glyph.width != 0 && glyph.height != 0) {
        y += font->header.ascent_A;
        font_render_glyph(font, &glyph, *x, y, context);
    }

    *x += glyph.pitch;
}

void u8g2_font_render_print(U8G2FontRender* font, int32_t x, int32_t y, const char* str, void* context) {
    while(*str) {
        const char* chr = str++;
        // if(*chr < 0x100) {
        u8g2_font_render_print_char(font, &x, y, *chr, context);
        // }
    }
}

void u8g2_font_render_print_slice(U8G2FontRender* font, int32_t x, int32_t y, const char* str, size_t len, void* context) {
    for(size_t i = 0; i < len; i++) {
        u8g2_font_render_print_char(font, &x, y, str[i], context);
    }
}

uint8_t font_get_unsigned_bits(U8G2FontGlyph* glyph, uint8_t count) {
    uint8_t val;
    uint8_t start = glyph->bit_pos;
    uint8_t end = start + count;

    val = *glyph->data;
    val >>= start;

    if(end >= 8) {
        uint8_t cnt = 8 - start;
        glyph->data++;

        val |= *glyph->data << (cnt);

        end -= 8;
    }

    glyph->bit_pos = end;

    val &= (1U << count) - 1;

    return val;
}

int8_t font_get_signed_bits(U8G2FontGlyph* glyph, uint8_t count) {
    int8_t val = (int8_t)font_get_unsigned_bits(glyph, count);
    val -= 1 << (count - 1);

    return val;
}

uint16_t font_get_start_symbol_search_postition(U8G2FontRender* font, char chr) {
    uint16_t search_position = U8G2_FONT_HEADER_SIZE;
    if(chr >= 65 && chr <= 90) {
        search_position = font->header.offset_A;
    } else if(chr >= 97 && chr <= 122) {
        search_position = font->header.offset_a;
    }

    return search_position;
}

int8_t font_get_glyph(U8G2FontRender* font, U8G2FontGlyph* glyph, uint16_t search_position, char chr) {
    while(1) {
        memcpy(glyph, font->data + search_position, 2);
        if(glyph->character == chr) {
            glyph->data = font->data + search_position + 2;
            glyph->bit_pos = 0;

            return U8G2FontRender_OK;
        }

        search_position += glyph->next_glypth;
        if(glyph->next_glypth == 0) {
            break;
        }
    }

    return U8G2FontRender_ERR;
}

void font_parse_glyph_header(U8G2FontRender* font, U8G2FontGlyph* glyph) {
    glyph->width = font_get_unsigned_bits(glyph, font->header.glyph_width);
    glyph->height = font_get_unsigned_bits(glyph, font->header.glyph_height);
    glyph->x_offset = font_get_signed_bits(glyph, font->header.glyph_x_offset);
    glyph->y_offset = font_get_signed_bits(glyph, font->header.glyph_y_offset);
    glyph->pitch = font_get_signed_bits(glyph, font->header.glyph_pitch);
}

int32_t font_draw_start_x_position(U8G2FontRender* font, U8G2FontGlyph* glyph) {
    (void)font;
    return glyph->x_offset;
}

int32_t font_draw_start_y_position(U8G2FontRender* font, U8G2FontGlyph* glyph) {
    (void)font;
    return -glyph->height - glyph->y_offset;
}

void font_render_glyph(U8G2FontRender* font, U8G2FontGlyph* glyph, int32_t x, int32_t y, void* context) {
    uint32_t pixels = 0;
    int32_t y_pos = y + font_draw_start_y_position(font, glyph);
    int32_t x_pos = x + font_draw_start_x_position(font, glyph);
    while(1) {
        uint8_t zeros = font_get_unsigned_bits(glyph, font->header.zero_bit_width);
        uint8_t ones = font_get_unsigned_bits(glyph, font->header.one_bit_width);
        int8_t repeat = 0;

        while(font_get_unsigned_bits(glyph, 1) == 1) {
            repeat++;
        }

        for(; repeat >= 0; repeat--) {
            for(uint8_t i = 0; i < zeros + ones; i++) {
                if(i <= zeros - 1) {
                    font->draw_pixel_bg(x_pos, y_pos, context);
                } else {
                    font->draw_pixel_fg(x_pos, y_pos, context);
                }
                x_pos++;

                pixels++;
                if(pixels % glyph->width == 0) {
                    y_pos++;
                    x_pos = x + font_draw_start_x_position(font, glyph);
                }
            }
        }

        if(pixels >= glyph->width * glyph->height) {
            break;
        }
    }
}

size_t u8g2_font_render_get_height(U8G2FontRender* font) {
    return font->header.ascent_A + 2;
}

static size_t u8g2_font_glyph_width(U8G2FontRender* font, char chr) {
    uint16_t search_position = font_get_start_symbol_search_postition(font, chr);

    U8G2FontGlyph glyph;
    if(font_get_glyph(font, &glyph, search_position, chr) != U8G2FontRender_OK) {
        return 0;
    }
    font_parse_glyph_header(font, &glyph);

    return glyph.pitch;
}

size_t u8g2_font_render_get_string_width(U8G2FontRender* font, const char* str, size_t len) {
    size_t width = 0;
    for(size_t i = 0; i < len; i++) {
        width += u8g2_font_glyph_width(font, str[i]);
    }
    return width;
}

void u8g2_font_render_print_multiline(U8G2FontRender* font, int32_t x, int32_t y, const char* str, size_t len, void* context) {
    int32_t original_x = x;

    for(size_t i = 0; i < len; i++) {
        if(str[i] == '\n') {
            y += u8g2_font_render_get_height(font);
            x = original_x;
        } else {
            u8g2_font_render_print_char(font, &x, y, str[i], context);
        }
    }
}

size_t u8g2_font_render_get_string_width_multiline(U8G2FontRender* font, const char* str, size_t len) {
    size_t max_width = 0;
    size_t line_width = 0;
    for(size_t i = 0; i < len; i++) {
        if(str[i] == '\n') {
            if(line_width > max_width) {
                max_width = line_width;
            }
            line_width = 0;
        } else {
            line_width += u8g2_font_glyph_width(font, str[i]);
        }
    }
    if(line_width > max_width) {
        max_width = line_width;
    }
    return max_width;
}
