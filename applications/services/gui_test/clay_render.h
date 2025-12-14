#pragma once
#include "clay.h"

typedef enum {
    FontBody,
    FontButton,
} Font;

void render_clear_buffer(uint8_t color);

Clay_Dimensions render_measure_text(Clay_StringSlice text, Clay_TextElementConfig* config, void* userData);

void render_rectangle(Clay_BoundingBox* bb, Clay_RectangleRenderData* rect_data);

void render_border(Clay_BoundingBox* bb, Clay_BorderRenderData* border_data);

void render_text(Clay_BoundingBox* bb, Clay_TextRenderData* text_data);

void render_image(Clay_BoundingBox* bb, Clay_ImageRenderData* image_data);

void render_scissor_start(Clay_BoundingBox* bb);

void render_scissor_end(void);

uint8_t* render_get_buffer(void);
