#pragma once
#include "clay.h"
#include "image.h"

typedef enum {
    FontBody,
    FontButton,
    FontKeyboard,

    // Special value, used to determine the size of the Font enum. Do not use.
    FontMax,
} Font;

typedef struct Canvas Canvas;

typedef uint8_t Color;

void canvas_init(void);

Canvas* canvas_alloc(size_t width, size_t height);

void canvas_free(Canvas* canvas);

void canvas_clear(Canvas* canvas, Color color);

Color* canvas_get_data(Canvas* canvas);

size_t canvas_get_width(Canvas* canvas);

size_t canvas_get_height(Canvas* canvas);

Clay_Dimensions clay_render_measure_text(Clay_StringSlice text, Clay_TextElementConfig* config, void* userData);

void clay_render_do_render(Canvas* canvas, Clay_RenderCommandArray* renderCommands);
