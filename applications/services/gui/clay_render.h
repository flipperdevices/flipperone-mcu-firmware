#pragma once
#include "clay.h"
#include "image.h"

typedef enum {
    FontBody, // haxrcorp4089
    FontButton, // helvB08
    FontKeyboard, // profont11
    FontBusy9, // busy9

    // Special value, used to determine the size of the Font enum. Do not use.
    FontMax,
} Font;

typedef struct Canvas Canvas;

typedef uint8_t Color;

typedef struct {
    Color color;
    uint8_t alpha;
} ColorDraw;

// must be called before any other canvas functions
void canvas_init(void);

Canvas* canvas_alloc(size_t width, size_t height);

Canvas* canvas_alloc_in_place(void* buffer, size_t width, size_t height);

size_t canvas_get_required_buffer_size(size_t width, size_t height);

void canvas_free(Canvas* canvas);

void canvas_clear(Canvas* canvas, Color color);

Color* canvas_get_data(Canvas* canvas);

size_t canvas_get_width(Canvas* canvas);

size_t canvas_get_height(Canvas* canvas);

Image canvas_to_image(Canvas* canvas);

// rendering primitives

void render_draw_line(Canvas* canvas, int32_t x0, int32_t y0, int32_t x1, int32_t y1, ColorDraw color);

void render_fill_round_rectangle(Canvas* canvas, int32_t x, int32_t y, int32_t width, int32_t height, int32_t radius, ColorDraw color);

void render_fill_round_rectangle_ext(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    int32_t radius_top_left,
    int32_t radius_top_right,
    int32_t radius_bottom_right,
    int32_t radius_bottom_left,
    ColorDraw color);

void render_draw_round_rectangle(Canvas* canvas, int32_t x, int32_t y, int32_t width, int32_t height, int32_t radius, int32_t border_width, ColorDraw color);

void render_draw_round_rectangle_ext(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    int32_t radius_top_left,
    int32_t radius_top_right,
    int32_t radius_bottom_right,
    int32_t radius_bottom_left,
    int32_t border_width_top,
    int32_t border_width_right,
    int32_t border_width_bottom,
    int32_t border_width_left,
    ColorDraw color);

// clay rendering API

Clay_Dimensions clay_render_measure_text(Clay_StringSlice text, Clay_TextElementConfig* config, void* userData);

void clay_render_do_render(Canvas* canvas, Clay_RenderCommandArray* renderCommands);
