#include <furi.h>
#include "clay_render.h"
#include "font/font_render.h"
#include "font/fonts.h"
#include <drivers/display/display_jd9853_reg.h>

#define TAG "render"

#define CANARY_VALUE 0xDEADBEEF
#define DATA_SIZE    (JD9853_WIDTH * JD9853_HEIGHT)

typedef uint8_t Color;

struct RenderBuffer {
    uint32_t canary_pre;
    uint8_t data[DATA_SIZE];
    uint32_t canary_post;
};

typedef struct {
    RenderBuffer* current_buffer;
    int32_t scissors_x0;
    int32_t scissors_y0;
    int32_t scissors_x1;
    int32_t scissors_y1;
} RenderData;

static RenderData render_data = {
    .current_buffer = NULL,
    .scissors_x0 = 0,
    .scissors_y0 = 0,
    .scissors_x1 = JD9853_WIDTH,
    .scissors_y1 = JD9853_HEIGHT,
};

static const void* render_get_font_by_id(Font font_id) {
    switch(font_id) {
    case FontBody:
        return u8g2_font_haxrcorp4089_tr;
    case FontButton:
        return u8g2_font_helvB08_tr;
    default:
        return u8g2_font_haxrcorp4089_tr;
    }
}

static inline Color render_color(Clay_Color textColor) {
    return textColor.r;
}

static inline Color render_color_darken(Color color, Color tint) {
    int32_t c = (int32_t)color - (int32_t)tint;
    if(c < 0) c = 0;
    return (Color)c;
}

static inline void render_set_pixel(int32_t x, int32_t y, Color color) {
    if(x < render_data.scissors_x0 || x >= render_data.scissors_x1 || y < render_data.scissors_y0 || y >= render_data.scissors_y1) {
        return;
    }
    render_data.current_buffer->data[y * JD9853_WIDTH + x] = color;
}

static inline void render_draw_hline(int32_t x0, int32_t y, int32_t x1, uint8_t color) {
    if(y < render_data.scissors_y0 || y >= render_data.scissors_y1) return;
    if(x0 < render_data.scissors_x0 && x1 < render_data.scissors_x0) return;
    if(x0 >= render_data.scissors_x1 && x1 >= render_data.scissors_x1) return;

    if(x0 > x1) M_SWAP(int32_t, x0, x1);
    if(x0 < render_data.scissors_x0) x0 = render_data.scissors_x0;
    if(x1 > render_data.scissors_x1) x1 = render_data.scissors_x1;
    if(x0 > x1) M_SWAP(int32_t, x0, x1);

    memset(&render_data.current_buffer->data[y * JD9853_WIDTH + x0], color, x1 - x0);
}

static inline void render_draw_vline(int32_t x, int32_t y0, int32_t y1, uint8_t color) {
    if(x < render_data.scissors_x0 || x >= render_data.scissors_x1) return;
    if(y0 < render_data.scissors_y0 && y1 < render_data.scissors_y0) return;
    if(y0 >= render_data.scissors_y1 && y1 >= render_data.scissors_y1) return;

    if(y0 > y1) M_SWAP(int32_t, y0, y1);
    if(y0 < render_data.scissors_y0) y0 = render_data.scissors_y0;
    if(y1 > render_data.scissors_y1) y1 = render_data.scissors_y1;
    if(y0 > y1) M_SWAP(int32_t, y0, y1);

    for(int32_t y = y0; y < y1; y++) {
        render_data.current_buffer->data[y * JD9853_WIDTH + x] = color;
    }
}

static inline void render_draw_circle(int32_t xc, int32_t yc, int32_t r, uint8_t color) {
    int32_t x = 0;
    int32_t y = r;
    int32_t d = 3 - 2 * r;
    while(x <= y) {
        render_set_pixel(xc + x, yc + y, color);
        render_set_pixel(xc - x, yc + y, color);
        render_set_pixel(xc + x, yc - y, color);
        render_set_pixel(xc - x, yc - y, color);
        render_set_pixel(xc + y, yc + x, color);
        render_set_pixel(xc - y, yc + x, color);
        render_set_pixel(xc + y, yc - x, color);
        render_set_pixel(xc - y, yc - x, color);
        if(d < 0) {
            d = d + 4 * x + 6;
        } else {
            d = d + 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

static inline void render_draw_circle_filled(int32_t xc, int32_t yc, int32_t r, uint8_t color) {
    int32_t x = 0;
    int32_t y = r;
    int32_t d = 3 - 2 * r;
    while(x <= y) {
        render_draw_hline(xc - x, yc - y, xc + x + 1, color);
        render_draw_hline(xc - x, yc + y, xc + x + 1, color);
        render_draw_hline(xc - y, yc - x, xc + y + 1, color);
        render_draw_hline(xc - y, yc + x, xc + y + 1, color);
        if(d < 0) {
            d = d + 4 * x + 6;
        } else {
            d = d + 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

static inline void render_draw_arc(int32_t xc, int32_t yc, int32_t r, float deg_start, float deg_stop, uint8_t color) {
    int32_t x = 0;
    int32_t y = r;
    int32_t d = 3 - 2 * r;

    // Normalize angles to [0, 360)
    while(deg_start < 0)
        deg_start += 360.0f;
    while(deg_stop < 0)
        deg_stop += 360.0f;
    deg_start = fmodf(deg_start, 360.0f);
    deg_stop = fmodf(deg_stop, 360.0f);

    while(x <= y) {
        // 8 octant points
        int32_t points[8][2] = {
            {xc + x, yc + y},
            {xc - x, yc + y},
            {xc + x, yc - y},
            {xc - x, yc - y},
            {xc + y, yc + x},
            {xc - y, yc + x},
            {xc + y, yc - x},
            {xc - y, yc - x},
        };

        for(int i = 0; i < 8; i++) {
            float angle = atan2f((float)(points[i][1] - yc), (float)(points[i][0] - xc)) * (180.0f / 3.14159265f);
            if(angle < 0) angle += 360.0f;

            // Check if angle is within arc range
            bool in_range = false;
            if(deg_start < deg_stop) {
                in_range = (angle >= deg_start && angle <= deg_stop);
            } else {
                in_range = (angle >= deg_start || angle <= deg_stop);
            }

            if(in_range) {
                render_set_pixel(points[i][0], points[i][1], color);
            }
        }

        if(d < 0) {
            d = d + 4 * x + 6;
        } else {
            d = d + 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

static inline void render_fill_arc(int32_t xc, int32_t yc, int32_t r, float deg_start, float deg_stop, uint8_t color) {
    while(deg_start < 0)
        deg_start += 360.0f;
    while(deg_stop < 0)
        deg_stop += 360.0f;
    deg_start = fmodf(deg_start, 360.0f);
    deg_stop = fmodf(deg_stop, 360.0f);

    for(int32_t y = -r; y <= r; y++) {
        for(int32_t x = -r; x <= r; x++) {
            int32_t dx = x;
            int32_t dy = y;
            if(dx * dx + dy * dy <= r * r) {
                float angle = atan2f((float)dy, (float)dx) * (180.0f / 3.14159265f);
                if(angle < 0) angle += 360.0f;

                // Check if angle is within arc range
                bool in_range = false;
                if(deg_start < deg_stop) {
                    in_range = (angle >= deg_start && angle <= deg_stop);
                } else {
                    in_range = (angle >= deg_start || angle <= deg_stop);
                }

                if(in_range) {
                    render_set_pixel(xc + x, yc + y, color);
                }
            }
        }
    }
}

static inline void render_draw_rectangle(int32_t x, int32_t y, int32_t width, int32_t height, uint8_t color) {
    render_draw_hline(x, y, x + width, color);
    render_draw_hline(x, y + height - 1, x + width, color);
    render_draw_vline(x, y, y + height, color);
    render_draw_vline(x + width - 1, y, y + height, color);
}

static inline void render_fill_rectangle(int32_t x, int32_t y, int32_t width, int32_t height, uint8_t color) {
    for(int32_t j = y; j < y + height; j++) {
        render_draw_hline(x, j, x + width, color);
    }
}

void render_clear_buffer(uint8_t color) {
    memset(render_data.current_buffer->data, color, DATA_SIZE);
}

static void render_draw_pixel_fg(int32_t x, int32_t y, void* context) {
    uint8_t color = *(uint8_t*)context;
    render_set_pixel(x, y, color);
}

static void render_draw_pixel_bg(int32_t x, int32_t y, void* context) {
    UNUSED(context);
    UNUSED(x);
    UNUSED(y);

    // render_set_pixel(x, y, render_data.bg_color);
}

static float render_clamp_corner_radius(float y_size, float radius) {
    if(radius < 1.0f) {
        return 0.0f;
    }
    if(radius > y_size / 2) {
        return y_size / 2;
    }
    // Trying to draw a 2x2 ellipse seems to result in just a dot, so if
    // there is a corner radius at minimum it must be 2
    return CLAY__MAX(2, radius);
}

static void render_rectangle(Clay_BoundingBox* bb, Clay_RectangleRenderData* rect_data) {
    uint32_t r_top_left = render_clamp_corner_radius(bb->height, rect_data->cornerRadius.topLeft);
    uint32_t r_top_right = render_clamp_corner_radius(bb->height, rect_data->cornerRadius.topRight);
    uint32_t r_bottom_right = render_clamp_corner_radius(bb->height, rect_data->cornerRadius.bottomRight);
    uint32_t r_bottom_left = render_clamp_corner_radius(bb->height, rect_data->cornerRadius.bottomLeft);
    uint8_t color = render_color(rect_data->backgroundColor);

    FURI_LOG_D(TAG, "Rectangle");
    FURI_LOG_D(TAG, "    [x: %.1f, y: %.1f, w: %.1f, h: %.1f] c%X", bb->x, bb->y, bb->width, bb->height, color);
    FURI_LOG_D(TAG, "    [%d, %d, %d, %d]", r_top_left, r_top_right, r_bottom_right, r_bottom_left);

    if(!r_top_left && !r_top_right && !r_bottom_right && !r_bottom_left) {
        render_fill_rectangle(bb->x, bb->y, bb->width, bb->height, color);
        return;
    }

    {
        render_fill_arc(bb->x + r_top_left, bb->y + r_top_left, r_top_left, 180.f, 270.f, color);
        render_fill_arc(bb->x + bb->width - r_top_right - 1, bb->y + r_top_right, r_top_right, 270.0f, 0.0f, color);
        render_fill_arc(bb->x + bb->width - r_bottom_right - 1, bb->y + bb->height - r_bottom_right - 1, r_bottom_right, 0.f, 90.f, color);
        render_fill_arc(bb->x + r_bottom_left, bb->y + bb->height - r_bottom_left - 1, r_bottom_left, 90.f, 180.f, color);
    }

    {
        render_fill_rectangle(bb->x + r_top_left, bb->y, bb->width - r_top_left - r_top_right, MAX(r_top_left, r_top_right), color);

        int32_t bottom_height = MAX(r_bottom_left, r_bottom_right);
        render_fill_rectangle(bb->x + r_bottom_left, bb->y + bb->height - bottom_height, bb->width - r_bottom_left - r_bottom_right, bottom_height, color);

        int32_t middle_height = bb->height - MIN(r_bottom_right, r_bottom_left) - MIN(r_top_right, r_top_left);
        render_fill_rectangle(
            bb->x + MIN(r_top_left, r_bottom_left), bb->y + MIN(r_top_right, r_top_left), bb->width - r_bottom_left - r_bottom_right, middle_height, color);

        int32_t left_height = bb->height - r_top_left - r_bottom_left;
        int32_t left_width = MAX(r_top_left, r_bottom_left);
        render_fill_rectangle(bb->x, bb->y + r_top_left, left_width, left_height, color);

        int32_t right_height = bb->height - r_top_right - r_bottom_right;
        int32_t right_width = MAX(r_top_right, r_bottom_right);
        render_fill_rectangle(bb->x + bb->width - right_width, bb->y + r_top_right, right_width, right_height, color);
    }
}

static void render_border(Clay_BoundingBox* bb, Clay_BorderRenderData* border_data) {
    uint32_t r_top_left = render_clamp_corner_radius(bb->height, border_data->cornerRadius.topLeft);
    uint32_t r_top_right = render_clamp_corner_radius(bb->height, border_data->cornerRadius.topRight);
    uint32_t r_bottom_right = render_clamp_corner_radius(bb->height, border_data->cornerRadius.bottomRight);
    uint32_t r_bottom_left = render_clamp_corner_radius(bb->height, border_data->cornerRadius.bottomLeft);
    uint8_t color = render_color(border_data->color);

    FURI_LOG_D(TAG, "Border");
    FURI_LOG_D(TAG, "    [x: %.1f, y: %.1f, w: %.1f, h: %.1f] c%X", bb->x, bb->y, bb->width, bb->height, color);
    FURI_LOG_D(TAG, "    [%d, %d, %d, %d]", r_top_left, r_top_right, r_bottom_right, r_bottom_left);
    FURI_LOG_D(TAG, "    [%d, %d, %d, %d]", border_data->width.top, border_data->width.right, border_data->width.bottom, border_data->width.left);

    if(border_data->width.top > 0) {
        render_draw_arc(bb->x + r_top_left, bb->y + r_top_left, r_top_left, 180.f, 270.f, color);
        render_fill_rectangle(bb->x + r_top_left, bb->y, bb->width - r_top_left - r_top_right, border_data->width.top, color);
        render_draw_arc(bb->x + bb->width - r_top_right - 1, bb->y + r_top_right, r_top_right, 270.f, 0.f, color);
    }

    if(border_data->width.right > 0 && r_top_right + r_bottom_right <= bb->height) {
        render_fill_rectangle(
            bb->x + bb->width - border_data->width.right, bb->y + r_top_right, border_data->width.right, bb->height - r_top_right - r_bottom_right, color);
    }

    if(border_data->width.bottom > 0) {
        render_draw_arc(bb->x + bb->width - r_bottom_right - 1, bb->y + bb->height - r_bottom_right - 1, r_bottom_right, 0.f, 90.f, color);
        render_fill_rectangle(
            bb->x + r_bottom_left, bb->y + bb->height - border_data->width.bottom, bb->width - r_bottom_left - r_bottom_right, border_data->width.bottom, color);
        render_draw_arc(bb->x + r_bottom_left, bb->y + bb->height - r_bottom_left - 1, r_bottom_left, 90.f, 180.f, color);
    }

    if(border_data->width.left > 0 && r_bottom_left + r_top_left < bb->height) {
        render_fill_rectangle(bb->x, bb->y + r_top_left, border_data->width.left, bb->height - r_top_left - r_bottom_left, color);
    }
}

static void render_text(Clay_BoundingBox* bb, Clay_TextRenderData* text_data) {
    FURI_LOG_D(TAG, "Text: '%.*s'", text_data->stringContents.length, text_data->stringContents.chars);
    FURI_LOG_D(TAG, "    [x: %.1f, y: %.1f, w: %.1f, h: %.1f] c%X", bb->x, bb->y, bb->width, bb->height, render_color(text_data->textColor));
    FURI_LOG_D(TAG, "    i[d: %d, size: %d, spacing: %d, line: %d]", text_data->fontId, text_data->fontSize, text_data->letterSpacing, text_data->lineHeight);

    uint8_t color = render_color(text_data->textColor);
    const void* font = render_get_font_by_id((Font)text_data->fontId);

    U8G2FontRender_t render = U8G2FontRender(font, render_draw_pixel_fg, render_draw_pixel_bg, &color);
    u8g2_render_print_multiline(&render, bb->x, bb->y, text_data->stringContents.chars, text_data->stringContents.length);
}

static void render_image(Clay_BoundingBox* bb, Clay_ImageRenderData* image_data) {
    Image* image = (Image*)image_data->imageData;
    furi_check(image);

    FURI_LOG_D(TAG, "Image");
    FURI_LOG_D(TAG, "    [x: %.1f, y: %.1f, w: %.1f, h: %.1f] c%X", bb->x, bb->y, bb->width, bb->height, render_color(image_data->backgroundColor));
    FURI_LOG_D(
        TAG,
        "    [%.1f, %.1f, %.1f, %.1f]",
        image_data->cornerRadius.topLeft,
        image_data->cornerRadius.topRight,
        image_data->cornerRadius.bottomRight,
        image_data->cornerRadius.bottomLeft);
    FURI_LOG_D(TAG, "    [img w: %d, h: %d]", image->width, image->height);

    uint8_t* data = image->data;
    switch(image->format) {
    case ImageFormatRawGray8: {
        for(uint32_t y = 0; y < MIN(image->height, bb->height); y++) {
            for(uint32_t x = 0; x < MIN(image->width, bb->width); x++) {
                uint8_t pixel = render_color_darken(data[y * image->width + x], image_data->backgroundColor.r);
                render_set_pixel(bb->x + x, bb->y + y, pixel);
            }
        }
        break;
    }
    default:
        FURI_LOG_E(TAG, "Unsupported image format: %d", image->format);
    }
}

static void render_scissor_start(Clay_BoundingBox* bb) {
    FURI_LOG_D(TAG, "Scissor start");
    FURI_LOG_D(TAG, "    [x: %.1f, y: %.1f, w: %.1f, h: %.1f]", bb->x, bb->y, bb->width, bb->height);
    render_data.scissors_x0 = bb->x;
    render_data.scissors_y0 = bb->y;
    render_data.scissors_x1 = bb->x + bb->width;
    render_data.scissors_y1 = bb->y + bb->height;

    // Clamp scissors to buffer dimensions
    if(render_data.scissors_x0 < 0) render_data.scissors_x0 = 0;
    if(render_data.scissors_y0 < 0) render_data.scissors_y0 = 0;
    if(render_data.scissors_x1 > JD9853_WIDTH) render_data.scissors_x1 = JD9853_WIDTH;
    if(render_data.scissors_y1 > JD9853_HEIGHT) render_data.scissors_y1 = JD9853_HEIGHT;
}

static void render_scissor_end(void) {
    FURI_LOG_D(TAG, "Scissor end");
    render_data.scissors_x0 = 0;
    render_data.scissors_y0 = 0;
    render_data.scissors_x1 = JD9853_WIDTH;
    render_data.scissors_y1 = JD9853_HEIGHT;
}

void render_do_render(Clay_RenderCommandArray* renderCommands) {
    for(int i = 0; i < renderCommands->length; i++) {
        Clay_RenderCommand* renderCommand = &renderCommands->internalArray[i];
        Clay_BoundingBox boundingBox = renderCommand->boundingBox;

        switch(renderCommand->commandType) {
        case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
            render_rectangle(&boundingBox, &renderCommand->renderData.rectangle);
            break;
        case CLAY_RENDER_COMMAND_TYPE_BORDER:
            render_border(&boundingBox, &renderCommand->renderData.border);
            break;
        case CLAY_RENDER_COMMAND_TYPE_TEXT:
            render_text(&boundingBox, &renderCommand->renderData.text);
            break;
        case CLAY_RENDER_COMMAND_TYPE_IMAGE:
            render_image(&boundingBox, &renderCommand->renderData.image);
            break;
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
            render_scissor_start(&boundingBox);
            break;
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
            render_scissor_end();
            break;
        case CLAY_RENDER_COMMAND_TYPE_CUSTOM:
            furi_crash("Custom render commands are not supported");
            break;
        }
    }
}

Clay_Dimensions render_measure_text(Clay_StringSlice text, Clay_TextElementConfig* config, void* userData) {
    const void* font = render_get_font_by_id((Font)config->fontId);
    U8G2FontRender_t font_render = U8G2FontRender(font, render_draw_pixel_fg, render_draw_pixel_bg, NULL);

    Clay_Dimensions dimensions = {
        .width = u8g2_font_get_string_width_multiline(&font_render, text.chars, text.length),
        .height = u8g2_font_get_height(&font_render),
    };

    return dimensions;
}

RenderBuffer* render_alloc_buffer(void) {
    RenderBuffer* buffer = malloc(sizeof(RenderBuffer));
    furi_check(buffer);
    buffer->canary_pre = CANARY_VALUE;
    buffer->canary_post = CANARY_VALUE;
    return buffer;
}

uint8_t* render_get_buffer_data(RenderBuffer* buffer) {
    furi_check(buffer->canary_pre == CANARY_VALUE, "RenderBuffer pre-canary corrupted");
    furi_check(buffer->canary_post == CANARY_VALUE, "RenderBuffer post-canary corrupted");
    return buffer->data;
}

RenderBuffer* render_get_current_buffer(void) {
    return render_data.current_buffer;
}

void render_set_current_buffer(RenderBuffer* buffer) {
    render_data.current_buffer = buffer;
}

size_t render_get_buffer_width(RenderBuffer* buffer) {
    UNUSED(buffer);
    return JD9853_WIDTH;
}

size_t render_get_buffer_height(RenderBuffer* buffer) {
    UNUSED(buffer);
    return JD9853_HEIGHT;
}