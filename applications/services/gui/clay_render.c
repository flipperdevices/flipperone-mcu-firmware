#include <furi.h>
#include "clay_render.h"
#include "font/font_render.h"
#include "font/fonts.h"

#define TAG "Render"

#define CANARY_VALUE 0xDEADBEEF

#ifdef RENDER_DEBUG_ENABLE
#define RENDER_DEBUG(...) FURI_LOG_I(__VA_ARGS__)
#else
#define RENDER_DEBUG(...)
#endif

struct Canvas {
    uint32_t* canary_pre;
    Color* data;
    uint32_t* canary_post;

    size_t width;
    size_t height;

    int32_t scissors_x0;
    int32_t scissors_y0;
    int32_t scissors_x1;
    int32_t scissors_y1;
};

typedef struct {
    Canvas* canvas;
    Color color;
} RenderTextContext;

static const void* render_get_font_by_id(Font font_id) {
    switch(font_id) {
    case FontBody:
        return u8g2_font_haxrcorp4089_tr;
    case FontButton:
        return u8g2_font_helvB08_tr;
    case FontKeyboard:
        return u8g2_font_profont11_mr;
    default:
        return u8g2_font_haxrcorp4089_tr;
    }
}

static inline Color render_color(Clay_Color color) {
    return color.r;
}

static inline Color render_color_darken(Color color, Color tint) {
    int32_t c = (int32_t)color - (int32_t)tint;
    if(c < 0) c = 0;
    return (Color)c;
}

static inline void render_set_pixel(Canvas* canvas, int32_t x, int32_t y, Color color) {
    if(x < canvas->scissors_x0 || x >= canvas->scissors_x1 || y < canvas->scissors_y0 || y >= canvas->scissors_y1) {
        return;
    }
    canvas->data[y * canvas->width + x] = color;
}

static void render_draw_pixel_fg(int32_t x, int32_t y, void* context) {
    RenderTextContext* ctx = context;
    render_set_pixel(ctx->canvas, x, y, ctx->color);
}

static void render_draw_pixel_bg(int32_t x, int32_t y, void* context) {
    UNUSED(context);
    UNUSED(x);
    UNUSED(y);
}

static inline void render_draw_hline(Canvas* canvas, int32_t x0, int32_t y, int32_t x1, Color color) {
    if(y < canvas->scissors_y0 || y >= canvas->scissors_y1) return;
    if(x0 < canvas->scissors_x0 && x1 < canvas->scissors_x0) return;
    if(x0 >= canvas->scissors_x1 && x1 >= canvas->scissors_x1) return;

    if(x0 > x1) M_SWAP(int32_t, x0, x1);
    if(x0 < canvas->scissors_x0) x0 = canvas->scissors_x0;
    if(x1 > canvas->scissors_x1) x1 = canvas->scissors_x1;
    if(x0 > x1) M_SWAP(int32_t, x0, x1);

    memset(&canvas->data[y * canvas->width + x0], color, x1 - x0);
}

static inline void render_draw_vline(Canvas* canvas, int32_t x, int32_t y0, int32_t y1, Color color) {
    if(x < canvas->scissors_x0 || x >= canvas->scissors_x1) return;
    if(y0 < canvas->scissors_y0 && y1 < canvas->scissors_y0) return;
    if(y0 >= canvas->scissors_y1 && y1 >= canvas->scissors_y1) return;

    if(y0 > y1) M_SWAP(int32_t, y0, y1);
    if(y0 < canvas->scissors_y0) y0 = canvas->scissors_y0;
    if(y1 > canvas->scissors_y1) y1 = canvas->scissors_y1;
    if(y0 > y1) M_SWAP(int32_t, y0, y1);

    for(int32_t y = y0; y < y1; y++) {
        canvas->data[y * canvas->width + x] = color;
    }
}

static inline void render_draw_circle(Canvas* canvas, int32_t xc, int32_t yc, int32_t r, Color color) {
    int32_t x = 0;
    int32_t y = r;
    int32_t d = 3 - 2 * r;
    while(x <= y) {
        render_set_pixel(canvas, xc + x, yc + y, color);
        render_set_pixel(canvas, xc - x, yc + y, color);
        render_set_pixel(canvas, xc + x, yc - y, color);
        render_set_pixel(canvas, xc - x, yc - y, color);
        render_set_pixel(canvas, xc + y, yc + x, color);
        render_set_pixel(canvas, xc - y, yc + x, color);
        render_set_pixel(canvas, xc + y, yc - x, color);
        render_set_pixel(canvas, xc - y, yc - x, color);
        if(d < 0) {
            d = d + 4 * x + 6;
        } else {
            d = d + 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

static inline void render_draw_circle_filled(Canvas* canvas, int32_t xc, int32_t yc, int32_t r, Color color) {
    int32_t x = 0;
    int32_t y = r;
    int32_t d = 3 - 2 * r;
    while(x <= y) {
        render_draw_hline(canvas, xc - x, yc - y, xc + x + 1, color);
        render_draw_hline(canvas, xc - x, yc + y, xc + x + 1, color);
        render_draw_hline(canvas, xc - y, yc - x, xc + y + 1, color);
        render_draw_hline(canvas, xc - y, yc + x, xc + y + 1, color);
        if(d < 0) {
            d = d + 4 * x + 6;
        } else {
            d = d + 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

static inline void render_draw_arc(Canvas* canvas, int32_t xc, int32_t yc, int32_t r, float deg_start, float deg_stop, Color color) {
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
                render_set_pixel(canvas, points[i][0], points[i][1], color);
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

static inline void render_fill_arc(Canvas* canvas, int32_t xc, int32_t yc, int32_t r, float deg_start, float deg_stop, Color color) {
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
                    render_set_pixel(canvas, xc + x, yc + y, color);
                }
            }
        }
    }
}

static inline void render_draw_rectangle(Canvas* canvas, int32_t x, int32_t y, int32_t width, int32_t height, Color color) {
    render_draw_hline(canvas, x, y, x + width, color);
    render_draw_hline(canvas, x, y + height - 1, x + width, color);
    render_draw_vline(canvas, x, y, y + height, color);
    render_draw_vline(canvas, x + width - 1, y, y + height, color);
}

static inline void render_fill_rectangle(Canvas* canvas, int32_t x, int32_t y, int32_t width, int32_t height, Color color) {
    for(int32_t j = y; j < y + height; j++) {
        render_draw_hline(canvas, x, j, x + width, color);
    }
}

void canvas_clear(Canvas* canvas, Color color) {
    if(sizeof(Color) == sizeof(uint8_t)) {
        memset(canvas->data, color, canvas->width * canvas->height);
    } else {
        for(size_t i = 0; i < (canvas->width * canvas->height); i++) {
            canvas->data[i] = color;
        }
    }
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

static void render_rectangle(Canvas* canvas, Clay_BoundingBox* bb, Clay_RectangleRenderData* rect_data) {
    uint32_t r_top_left = render_clamp_corner_radius(bb->height, rect_data->cornerRadius.topLeft);
    uint32_t r_top_right = render_clamp_corner_radius(bb->height, rect_data->cornerRadius.topRight);
    uint32_t r_bottom_right = render_clamp_corner_radius(bb->height, rect_data->cornerRadius.bottomRight);
    uint32_t r_bottom_left = render_clamp_corner_radius(bb->height, rect_data->cornerRadius.bottomLeft);
    Color color = render_color(rect_data->backgroundColor);

    RENDER_DEBUG(TAG, "Rectangle");
    RENDER_DEBUG(TAG, "    [x: %.1f, y: %.1f, w: %.1f, h: %.1f] c%X", bb->x, bb->y, bb->width, bb->height, color);
    RENDER_DEBUG(TAG, "    [%lu, %lu, %lu, %lu]", r_top_left, r_top_right, r_bottom_right, r_bottom_left);

    if(!r_top_left && !r_top_right && !r_bottom_right && !r_bottom_left) {
        render_fill_rectangle(canvas, bb->x, bb->y, bb->width, bb->height, color);
        return;
    }

    {
        render_fill_arc(canvas, bb->x + r_top_left, bb->y + r_top_left, r_top_left, 180.f, 270.f, color);
        render_fill_arc(canvas, bb->x + bb->width - r_top_right - 1, bb->y + r_top_right, r_top_right, 270.0f, 0.0f, color);
        render_fill_arc(canvas, bb->x + bb->width - r_bottom_right - 1, bb->y + bb->height - r_bottom_right - 1, r_bottom_right, 0.f, 90.f, color);
        render_fill_arc(canvas, bb->x + r_bottom_left, bb->y + bb->height - r_bottom_left - 1, r_bottom_left, 90.f, 180.f, color);
    }

    {
        render_fill_rectangle(canvas, bb->x + r_top_left, bb->y, bb->width - r_top_left - r_top_right, MAX(r_top_left, r_top_right), color);

        int32_t bottom_height = MAX(r_bottom_left, r_bottom_right);
        render_fill_rectangle(
            canvas, bb->x + r_bottom_left, bb->y + bb->height - bottom_height, bb->width - r_bottom_left - r_bottom_right, bottom_height, color);

        int32_t middle_height = bb->height - MIN(r_bottom_right, r_bottom_left) - MIN(r_top_right, r_top_left);
        render_fill_rectangle(
            canvas,
            bb->x + MIN(r_top_left, r_bottom_left),
            bb->y + MIN(r_top_right, r_top_left),
            bb->width - r_bottom_left - r_bottom_right,
            middle_height,
            color);

        int32_t left_height = bb->height - r_top_left - r_bottom_left;
        int32_t left_width = MAX(r_top_left, r_bottom_left);
        render_fill_rectangle(canvas, bb->x, bb->y + r_top_left, left_width, left_height, color);

        int32_t right_height = bb->height - r_top_right - r_bottom_right;
        int32_t right_width = MAX(r_top_right, r_bottom_right);
        render_fill_rectangle(canvas, bb->x + bb->width - right_width, bb->y + r_top_right, right_width, right_height, color);
    }
}

static void render_border(Canvas* canvas, Clay_BoundingBox* bb, Clay_BorderRenderData* border_data) {
    uint32_t r_top_left = render_clamp_corner_radius(bb->height, border_data->cornerRadius.topLeft);
    uint32_t r_top_right = render_clamp_corner_radius(bb->height, border_data->cornerRadius.topRight);
    uint32_t r_bottom_right = render_clamp_corner_radius(bb->height, border_data->cornerRadius.bottomRight);
    uint32_t r_bottom_left = render_clamp_corner_radius(bb->height, border_data->cornerRadius.bottomLeft);
    Color color = render_color(border_data->color);

    RENDER_DEBUG(TAG, "Border");
    RENDER_DEBUG(TAG, "    [x: %.1f, y: %.1f, w: %.1f, h: %.1f] c%X", bb->x, bb->y, bb->width, bb->height, color);
    RENDER_DEBUG(TAG, "    [%lu, %lu, %lu, %lu]", r_top_left, r_top_right, r_bottom_right, r_bottom_left);
    RENDER_DEBUG(TAG, "    [%d, %d, %d, %d]", border_data->width.top, border_data->width.right, border_data->width.bottom, border_data->width.left);

    if(border_data->width.top > 0) {
        render_draw_arc(canvas, bb->x + r_top_left, bb->y + r_top_left, r_top_left, 180.f, 270.f, color);
        render_fill_rectangle(canvas, bb->x + r_top_left, bb->y, bb->width - r_top_left - r_top_right, border_data->width.top, color);
        render_draw_arc(canvas, bb->x + bb->width - r_top_right - 1, bb->y + r_top_right, r_top_right, 270.f, 0.f, color);
    }

    if(border_data->width.right > 0 && r_top_right + r_bottom_right <= bb->height) {
        render_fill_rectangle(
            canvas,
            bb->x + bb->width - border_data->width.right,
            bb->y + r_top_right,
            border_data->width.right,
            bb->height - r_top_right - r_bottom_right,
            color);
    }

    if(border_data->width.bottom > 0) {
        render_draw_arc(canvas, bb->x + bb->width - r_bottom_right - 1, bb->y + bb->height - r_bottom_right - 1, r_bottom_right, 0.f, 90.f, color);
        render_fill_rectangle(
            canvas,
            bb->x + r_bottom_left,
            bb->y + bb->height - border_data->width.bottom,
            bb->width - r_bottom_left - r_bottom_right,
            border_data->width.bottom,
            color);
        render_draw_arc(canvas, bb->x + r_bottom_left, bb->y + bb->height - r_bottom_left - 1, r_bottom_left, 90.f, 180.f, color);
    }

    if(border_data->width.left > 0 && r_bottom_left + r_top_left < bb->height) {
        render_fill_rectangle(canvas, bb->x, bb->y + r_top_left, border_data->width.left, bb->height - r_top_left - r_bottom_left, color);
    }
}

static void render_text(Canvas* canvas, Clay_BoundingBox* bb, Clay_TextRenderData* text_data) {
    RENDER_DEBUG(TAG, "Text: '%.*s'", (int)text_data->stringContents.length, text_data->stringContents.chars);
    RENDER_DEBUG(TAG, "    [x: %.1f, y: %.1f, w: %.1f, h: %.1f] c%X", bb->x, bb->y, bb->width, bb->height, render_color(text_data->textColor));
    RENDER_DEBUG(TAG, "    i[d: %d, size: %d, spacing: %d, line: %d]", text_data->fontId, text_data->fontSize, text_data->letterSpacing, text_data->lineHeight);

    Color color = render_color(text_data->textColor);
    const void* font = render_get_font_by_id((Font)text_data->fontId);

    RenderTextContext ctx = {
        .canvas = canvas,
        .color = color,
    };
    U8G2FontRender_t render = U8G2FontRender(font, render_draw_pixel_fg, render_draw_pixel_bg, &ctx);

    u8g2_render_print_multiline(&render, bb->x, bb->y, text_data->stringContents.chars, text_data->stringContents.length);
}

static void render_image(Canvas* canvas, Clay_BoundingBox* bb, Clay_ImageRenderData* image_data) {
    Image* image = (Image*)image_data->imageData;
    furi_check(image);

    RENDER_DEBUG(TAG, "Image");
    RENDER_DEBUG(TAG, "    [x: %.1f, y: %.1f, w: %.1f, h: %.1f] c%X", bb->x, bb->y, bb->width, bb->height, render_color(image_data->backgroundColor));
    RENDER_DEBUG(
        TAG,
        "    [%.1f, %.1f, %.1f, %.1f]",
        image_data->cornerRadius.topLeft,
        image_data->cornerRadius.topRight,
        image_data->cornerRadius.bottomRight,
        image_data->cornerRadius.bottomLeft);
    RENDER_DEBUG(TAG, "    [img w: %lu, h: %lu]", image->width, image->height);

    const uint8_t* data = image->data;
    switch(image->format) {
    case ImageFormatRawGray8: {
        for(uint32_t y = 0; y < MIN(image->height, bb->height); y++) {
            for(uint32_t x = 0; x < MIN(image->width, bb->width); x++) {
                uint8_t pixel = render_color_darken(data[y * image->width + x], image_data->backgroundColor.r);
                render_set_pixel(canvas, bb->x + x, bb->y + y, pixel);
            }
        }
        break;
    }
    default:
        FURI_LOG_E(TAG, "Unsupported image format: %d", image->format);
    }
}

static void render_scissor_start(Canvas* canvas, Clay_BoundingBox* bb) {
    RENDER_DEBUG(TAG, "Scissor start");
    RENDER_DEBUG(TAG, "    [x: %.1f, y: %.1f, w: %.1f, h: %.1f]", bb->x, bb->y, bb->width, bb->height);

    canvas->scissors_x0 = bb->x;
    canvas->scissors_y0 = bb->y;
    canvas->scissors_x1 = bb->x + bb->width;
    canvas->scissors_y1 = bb->y + bb->height;

    // Clamp scissors to canvas dimensions
    if(canvas->scissors_x0 < 0) canvas->scissors_x0 = 0;
    if(canvas->scissors_y0 < 0) canvas->scissors_y0 = 0;
    if(canvas->scissors_x1 > (int32_t)canvas->width) canvas->scissors_x1 = canvas->width;
    if(canvas->scissors_y1 > (int32_t)canvas->height) canvas->scissors_y1 = canvas->height;
}

static void render_scissor_end(Canvas* canvas) {
    RENDER_DEBUG(TAG, "Scissor end");
    canvas->scissors_x0 = 0;
    canvas->scissors_y0 = 0;
    canvas->scissors_x1 = canvas->width;
    canvas->scissors_y1 = canvas->height;
}

void render_do_render(Canvas* canvas, Clay_RenderCommandArray* renderCommands) {
    for(int i = 0; i < renderCommands->length; i++) {
        Clay_RenderCommand* renderCommand = &renderCommands->internalArray[i];
        Clay_BoundingBox boundingBox = renderCommand->boundingBox;

        switch(renderCommand->commandType) {
        case CLAY_RENDER_COMMAND_TYPE_NONE:
            break;
        case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
            render_rectangle(canvas, &boundingBox, &renderCommand->renderData.rectangle);
            break;
        case CLAY_RENDER_COMMAND_TYPE_BORDER:
            render_border(canvas, &boundingBox, &renderCommand->renderData.border);
            break;
        case CLAY_RENDER_COMMAND_TYPE_TEXT:
            render_text(canvas, &boundingBox, &renderCommand->renderData.text);
            break;
        case CLAY_RENDER_COMMAND_TYPE_IMAGE:
            render_image(canvas, &boundingBox, &renderCommand->renderData.image);
            break;
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
            render_scissor_start(canvas, &boundingBox);
            break;
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
            render_scissor_end(canvas);
            break;
        case CLAY_RENDER_COMMAND_TYPE_CUSTOM:
            furi_crash("Custom render commands are not supported");
            break;
        }
    }
}

Clay_Dimensions render_measure_text(Clay_StringSlice text, Clay_TextElementConfig* config, void* userData) {
    UNUSED(userData);
    const void* font = render_get_font_by_id((Font)config->fontId);
    RenderTextContext ctx = {
        .canvas = NULL,
        .color = 0,
    };
    U8G2FontRender_t font_render = U8G2FontRender(font, render_draw_pixel_fg, render_draw_pixel_bg, &ctx);

    Clay_Dimensions dimensions = {
        .width = u8g2_font_get_string_width_multiline(&font_render, text.chars, text.length),
        .height = u8g2_font_get_height(&font_render),
    };

    return dimensions;
}

Canvas* canvas_alloc(size_t width, size_t height) {
    Canvas* canvas = malloc(sizeof(Canvas));
    canvas->canary_pre = malloc(sizeof(uint32_t) + sizeof(Color) * width * height + sizeof(uint32_t));
    canvas->data = (Color*)(canvas->canary_pre + 1);
    canvas->canary_post = (uint32_t*)(canvas->data + width * height);
    *(canvas->canary_pre) = CANARY_VALUE;
    *(canvas->canary_post) = CANARY_VALUE;
    canvas->width = width;
    canvas->height = height;

    return canvas;
}

void canvas_free(Canvas* canvas) {
    free(canvas->canary_pre);
    free(canvas);
}

Color* canvas_get_data(Canvas* canvas) {
    furi_check(*canvas->canary_pre == CANARY_VALUE, "Canvas pre-canary corrupted");
    furi_check(*canvas->canary_post == CANARY_VALUE, "Canvas post-canary corrupted");
    return canvas->data;
}

size_t canvas_get_width(Canvas* canvas) {
    return canvas->width;
}

size_t canvas_get_height(Canvas* canvas) {
    return canvas->height;
}
