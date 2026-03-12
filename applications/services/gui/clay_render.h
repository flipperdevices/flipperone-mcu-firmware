#pragma once
#include "clay.h"
#include "image.h"

typedef enum {
    FontBody,
    FontButton,
    FontKeyboard,
} Font;

typedef struct Canvas Canvas;

typedef uint8_t Color;

void canvas_clear(Canvas* canvas, Color color);

Clay_Dimensions render_measure_text(Clay_StringSlice text, Clay_TextElementConfig* config, void* userData);

Canvas* canvas_alloc(size_t width, size_t height);

void canvas_free(Canvas* canvas);

Color* canvas_get_data(Canvas* canvas);

size_t canvas_get_width(Canvas* canvas);

size_t canvas_get_height(Canvas* canvas);

void render_do_render(Canvas* canvas, Clay_RenderCommandArray* renderCommands);
