#pragma once
#include "clay.h"

typedef enum {
    FontBody,
    FontButton,
    FontKeyboard,
} Font;

typedef struct RenderBuffer RenderBuffer;

typedef enum {
    ImageFormatRawGray8,
} ImageFormat;

typedef struct {
    ImageFormat format;
    uint32_t width;
    uint32_t height;
    void* data;
} Image;

void render_clear_buffer(uint8_t color);

Clay_Dimensions render_measure_text(Clay_StringSlice text, Clay_TextElementConfig* config, void* userData);

RenderBuffer* render_alloc_buffer(void);

uint8_t* render_get_buffer_data(RenderBuffer* buffer);

RenderBuffer* render_get_current_buffer(void);

void render_set_current_buffer(RenderBuffer* buffer);

size_t render_get_buffer_width(RenderBuffer* buffer);

size_t render_get_buffer_height(RenderBuffer* buffer);

void render_do_render(Clay_RenderCommandArray* renderCommands);