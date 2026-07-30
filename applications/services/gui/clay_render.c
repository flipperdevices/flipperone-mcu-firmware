#include <furi.h>
#include <string.h>
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

    bool allocated_by_malloc;
};

typedef struct {
    Canvas* canvas;
    ColorA color;
} RenderTextContext;

static U8G2FontRender render_font_renderers[FontMax];

static const void* render_get_font_by_id(Font font_id) {
    switch(font_id) {
    case FontBody:
        return u8g2_font_haxrcorp4089_tr;
    case FontButton:
        return u8g2_font_helvB08_tr;
    case FontKeyboard:
        return u8g2_font_profont11_mr;
    case FontBusy9:
        return u8g2_font_busy9_tr;
    case FontBig:
        return u8g2_font_born2bsporty2_tn;
    default:
        return u8g2_font_haxrcorp4089_tr;
    }
}

static inline ColorA render_color(Clay_Color color) {
    return (ColorA){.color = color.r, .alpha = color.a};
}

static inline void render_set_pixel_unsafe(Canvas* canvas, int32_t x, int32_t y, ColorA color) {
    uint32_t dst = canvas->data[y * canvas->width + x];
    uint32_t src = color.color;
    uint8_t alpha = color.alpha;
    uint8_t inv_alpha = 255 - alpha;

    uint32_t blended = src * alpha + dst * inv_alpha + 128;
    canvas->data[y * canvas->width + x] = (blended + (blended >> 8)) >> 8;
}

static inline void render_set_pixel(Canvas* canvas, int32_t x, int32_t y, ColorA color) {
    if(x < canvas->scissors_x0 || x >= canvas->scissors_x1 || y < canvas->scissors_y0 || y >= canvas->scissors_y1) {
        return;
    }

    switch(color.alpha) {
    case 0:
        break;
    case 255:
        canvas->data[y * canvas->width + x] = color.color;
        break;
    default:
        render_set_pixel_unsafe(canvas, x, y, color);
        break;
    }
}

static void render_draw_pixel_fg(int32_t x, int32_t y, void* context) {
    RenderTextContext* ctx = context;
    render_set_pixel(ctx->canvas, x, y, ctx->color);
}

static inline void render_draw_hline(Canvas* canvas, int32_t x0, int32_t y, int32_t x1, ColorA color) {
    if(y < canvas->scissors_y0 || y >= canvas->scissors_y1) return;
    if(x0 < canvas->scissors_x0 && x1 < canvas->scissors_x0) return;
    if(x0 >= canvas->scissors_x1 && x1 >= canvas->scissors_x1) return;

    if(x0 > x1) M_SWAP(int32_t, x0, x1);
    if(x0 < canvas->scissors_x0) x0 = canvas->scissors_x0;
    if(x1 > canvas->scissors_x1) x1 = canvas->scissors_x1;
    if(x0 > x1) M_SWAP(int32_t, x0, x1);

    switch(color.alpha) {
    case 0:
        break;
    case 255:
        memset(&canvas->data[y * canvas->width + x0], color.color, x1 - x0);
        break;
    default:
        for(int32_t x = x0; x < x1; x++) {
            render_set_pixel_unsafe(canvas, x, y, color);
        }
    }
}

static inline void render_draw_vline(Canvas* canvas, int32_t x, int32_t y0, int32_t y1, ColorA color) {
    if(x < canvas->scissors_x0 || x >= canvas->scissors_x1) return;
    if(y0 < canvas->scissors_y0 && y1 < canvas->scissors_y0) return;
    if(y0 >= canvas->scissors_y1 && y1 >= canvas->scissors_y1) return;

    if(y0 > y1) M_SWAP(int32_t, y0, y1);
    if(y0 < canvas->scissors_y0) y0 = canvas->scissors_y0;
    if(y1 > canvas->scissors_y1) y1 = canvas->scissors_y1;
    if(y0 > y1) M_SWAP(int32_t, y0, y1);

    for(int32_t y = y0; y < y1; y++) {
        switch(color.alpha) {
        case 0:
            break;
        case 255:
            canvas->data[y * canvas->width + x] = color.color;
            break;
        default:
            render_set_pixel_unsafe(canvas, x, y, color);
            break;
        }
    }
}

static inline void render_draw_circle(Canvas* canvas, int32_t xc, int32_t yc, int32_t r, ColorA color) {
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

static inline void render_draw_circle_filled(Canvas* canvas, int32_t xc, int32_t yc, int32_t r, ColorA color) {
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

static inline void render_draw_arc(Canvas* canvas, int32_t xc, int32_t yc, int32_t r, float deg_start, float deg_stop, ColorA color) {
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

static inline void render_fill_arc(Canvas* canvas, int32_t xc, int32_t yc, int32_t r, float deg_start, float deg_stop, ColorA color) {
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

static inline void render_draw_rectangle(Canvas* canvas, int32_t x, int32_t y, int32_t width, int32_t height, ColorA color) {
    render_draw_hline(canvas, x, y, x + width, color);
    render_draw_hline(canvas, x, y + height - 1, x + width, color);
    render_draw_vline(canvas, x, y, y + height, color);
    render_draw_vline(canvas, x + width - 1, y, y + height, color);
}

static inline void render_fill_rectangle(Canvas* canvas, int32_t x, int32_t y, int32_t width, int32_t height, ColorA color) {
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

void render_draw_line(Canvas* canvas, int32_t x0, int32_t y0, int32_t x1, int32_t y1, ColorA color) {
    // classic bresenham's line algorithm
    int32_t dx = abs(x1 - x0);
    int32_t dy = abs(y1 - y0);
    int32_t sx = (x0 < x1) ? 1 : -1;
    int32_t sy = (y0 < y1) ? 1 : -1;
    int32_t err = dx - dy;

    while(true) {
        render_set_pixel(canvas, x0, y0, color);
        if(x0 == x1 && y0 == y1) break;
        int32_t err2 = err * 2;
        if(err2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if(err2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void render_fill_round_rectangle(Canvas* canvas, int32_t x, int32_t y, int32_t width, int32_t height, int32_t radius, ColorA color) {
    render_fill_round_rectangle_ext(canvas, x, y, width, height, radius, radius, radius, radius, color);
}

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
    ColorA color) {
    if(color.alpha == 0) {
        return;
    }

    uint32_t r_top_left = render_clamp_corner_radius(height, radius_top_left);
    uint32_t r_top_right = render_clamp_corner_radius(height, radius_top_right);
    uint32_t r_bottom_right = render_clamp_corner_radius(height, radius_bottom_right);
    uint32_t r_bottom_left = render_clamp_corner_radius(height, radius_bottom_left);

    RENDER_DEBUG(TAG, "Rectangle");
    RENDER_DEBUG(TAG, "    [x: %.1f, y: %.1f, w: %.1f, h: %.1f] c%X", x, y, width, height, color);
    RENDER_DEBUG(TAG, "    [%lu, %lu, %lu, %lu]", r_top_left, r_top_right, r_bottom_right, r_bottom_left);

    if(!r_top_left && !r_top_right && !r_bottom_right && !r_bottom_left) {
        render_fill_rectangle(canvas, x, y, width, height, color);
        return;
    }

    {
        render_fill_arc(canvas, x + r_top_left, y + r_top_left, r_top_left, 180.f, 270.f, color);
        render_fill_arc(canvas, x + width - r_top_right - 1, y + r_top_right, r_top_right, 270.0f, 0.0f, color);
        render_fill_arc(canvas, x + width - r_bottom_right - 1, y + height - r_bottom_right - 1, r_bottom_right, 0.f, 90.f, color);
        render_fill_arc(canvas, x + r_bottom_left, y + height - r_bottom_left - 1, r_bottom_left, 90.f, 180.f, color);
    }

    {
        render_fill_rectangle(canvas, x + r_top_left, y, width - r_top_left - r_top_right, MAX(r_top_left, r_top_right), color);

        int32_t bottom_height = MAX(r_bottom_left, r_bottom_right);
        render_fill_rectangle(canvas, x + r_bottom_left, y + height - bottom_height, width - r_bottom_left - r_bottom_right, bottom_height, color);

        int32_t middle_height = height - MIN(r_bottom_right, r_bottom_left) - MIN(r_top_right, r_top_left);
        render_fill_rectangle(
            canvas, x + MIN(r_top_left, r_bottom_left), y + MIN(r_top_right, r_top_left), width - r_bottom_left - r_bottom_right, middle_height, color);

        int32_t left_height = height - r_top_left - r_bottom_left;
        int32_t left_width = MAX(r_top_left, r_bottom_left);
        render_fill_rectangle(canvas, x, y + r_top_left, left_width, left_height, color);

        int32_t right_height = height - r_top_right - r_bottom_right;
        int32_t right_width = MAX(r_top_right, r_bottom_right);
        render_fill_rectangle(canvas, x + width - right_width, y + r_top_right, right_width, right_height, color);
    }
}

void render_draw_round_rectangle(Canvas* canvas, int32_t x, int32_t y, int32_t width, int32_t height, int32_t radius, int32_t border_width, ColorA color) {
    render_draw_round_rectangle_ext(canvas, x, y, width, height, radius, radius, radius, radius, border_width, border_width, border_width, border_width, color);
}

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
    ColorA color) {
    if(color.alpha == 0) {
        return;
    }

    uint32_t r_top_left = render_clamp_corner_radius(height, radius_top_left);
    uint32_t r_top_right = render_clamp_corner_radius(height, radius_top_right);
    uint32_t r_bottom_right = render_clamp_corner_radius(height, radius_bottom_right);
    uint32_t r_bottom_left = render_clamp_corner_radius(height, radius_bottom_left);

    RENDER_DEBUG(TAG, "Border");
    RENDER_DEBUG(TAG, "    [x: %.1f, y: %.1f, w: %.1f, h: %.1f] c%X", x, y, width, height, color);
    RENDER_DEBUG(TAG, "    [%lu, %lu, %lu, %lu]", r_top_left, r_top_right, r_bottom_right, r_bottom_left);
    RENDER_DEBUG(TAG, "    [%d, %d, %d, %d]", border_width_top, border_width_right, border_width_bottom, border_width_left);

    if(border_width_top > 0) {
        render_draw_arc(canvas, x + r_top_left, y + r_top_left, r_top_left, 180.f, 270.f, color);
        render_fill_rectangle(canvas, x + r_top_left, y, width - r_top_left - r_top_right, border_width_top, color);
        render_draw_arc(canvas, x + width - r_top_right - 1, y + r_top_right, r_top_right, 270.f, 0.f, color);
    }

    if(border_width_right > 0 && r_top_right + r_bottom_right <= height) {
        render_fill_rectangle(canvas, x + width - border_width_right, y + r_top_right, border_width_right, height - r_top_right - r_bottom_right, color);
    }

    if(border_width_bottom > 0) {
        render_draw_arc(canvas, x + width - r_bottom_right - 1, y + height - r_bottom_right - 1, r_bottom_right, 0.f, 90.f, color);
        render_fill_rectangle(canvas, x + r_bottom_left, y + height - border_width_bottom, width - r_bottom_left - r_bottom_right, border_width_bottom, color);
        render_draw_arc(canvas, x + r_bottom_left, y + height - r_bottom_left - 1, r_bottom_left, 90.f, 180.f, color);
    }

    if(border_width_left > 0 && r_bottom_left + r_top_left < height) {
        render_fill_rectangle(canvas, x, y + r_top_left, border_width_left, height - r_top_left - r_bottom_left, color);
    }
}

static void render_text(Canvas* canvas, Clay_BoundingBox* bb, Clay_TextRenderData* text_data) {
    RENDER_DEBUG(TAG, "Text: '%.*s'", (int)text_data->stringContents.length, text_data->stringContents.chars);
    RENDER_DEBUG(TAG, "    [x: %.1f, y: %.1f, w: %.1f, h: %.1f] c%X", bb->x, bb->y, bb->width, bb->height, render_color(text_data->textColor));
    RENDER_DEBUG(TAG, "    i[d: %d, size: %d, spacing: %d, line: %d]", text_data->fontId, text_data->fontSize, text_data->letterSpacing, text_data->lineHeight);

    RenderTextContext ctx = {
        .canvas = canvas,
        .color = render_color(text_data->textColor),
    };

    if(ctx.color.alpha == 0) {
        return;
    }

    furi_check(text_data->fontId < FontMax);

    U8G2FontRender* font_render = &render_font_renderers[text_data->fontId];

    u8g2_font_render_print_multiline(font_render, bb->x, bb->y, text_data->stringContents.chars, text_data->stringContents.length, &ctx);
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
        // Clip source dimensions to the target bounding box
        int32_t src_w = (int32_t)MIN(image->width, bb->width);
        int32_t src_h = (int32_t)MIN(image->height, bb->height);
        int32_t dst_x0 = (int32_t)bb->x;
        int32_t dst_y0 = (int32_t)bb->y;

        /*
         * Fast path (no scissor clipping needed):
         *
         * The source image is grayscale (1 byte/pixel) and fully opaque
         * (alpha = 255).  Since no alpha blending is required we can
         * memcpy entire rows at once — much faster than calling
         * render_set_pixel() once per pixel (which does bounds checks
         * and blending math even for opaque pixels).
         *
         * Slow path (with scissor clipping):
         * Fall back to pixel-by-pixel rendering which respects the
         * scissor rectangle.
         */
        if(dst_x0 >= canvas->scissors_x0 && dst_y0 >= canvas->scissors_y0 &&
           dst_x0 + src_w <= canvas->scissors_x1 && dst_y0 + src_h <= canvas->scissors_y1) {
            for(int32_t y = 0; y < src_h; y++) {
                memcpy(
                    &canvas->data[(dst_y0 + y) * canvas->width + dst_x0],
                    &data[y * image->width],
                    src_w);
            }
        } else {
            for(int32_t y = 0; y < src_h; y++) {
                for(int32_t x = 0; x < src_w; x++) {
                    uint8_t pixel = data[y * image->width + x];
                    render_set_pixel(canvas, dst_x0 + x, dst_y0 + y, (ColorA){.color = pixel, .alpha = 255});
                }
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

static void render_scissor_reset(Canvas* canvas) {
    RENDER_DEBUG(TAG, "Scissor reset");
    canvas->scissors_x0 = 0;
    canvas->scissors_y0 = 0;
    canvas->scissors_x1 = canvas->width;
    canvas->scissors_y1 = canvas->height;
}

void clay_render_do_render(Canvas* canvas, Clay_RenderCommandArray* render_commands) {
    for(int i = 0; i < render_commands->length; i++) {
        Clay_RenderCommand* render_command = &render_commands->internalArray[i];
        Clay_BoundingBox bounding_box = render_command->boundingBox;

        switch(render_command->commandType) {
        case CLAY_RENDER_COMMAND_TYPE_NONE:
            break;
        case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
            Clay_RectangleRenderData* rectangle_data = &render_command->renderData.rectangle;
            render_fill_round_rectangle_ext(
                canvas,
                bounding_box.x,
                bounding_box.y,
                bounding_box.width,
                bounding_box.height,
                rectangle_data->cornerRadius.topLeft,
                rectangle_data->cornerRadius.topRight,
                rectangle_data->cornerRadius.bottomRight,
                rectangle_data->cornerRadius.bottomLeft,
                render_color(rectangle_data->backgroundColor));
            break;
        case CLAY_RENDER_COMMAND_TYPE_BORDER:
            Clay_BorderRenderData* border_data = &render_command->renderData.border;
            render_draw_round_rectangle_ext(
                canvas,
                bounding_box.x,
                bounding_box.y,
                bounding_box.width,
                bounding_box.height,
                border_data->cornerRadius.topLeft,
                border_data->cornerRadius.topRight,
                border_data->cornerRadius.bottomRight,
                border_data->cornerRadius.bottomLeft,
                border_data->width.top,
                border_data->width.right,
                border_data->width.bottom,
                border_data->width.left,
                render_color(border_data->color));
            break;
        case CLAY_RENDER_COMMAND_TYPE_TEXT:
            render_text(canvas, &bounding_box, &render_command->renderData.text);
            break;
        case CLAY_RENDER_COMMAND_TYPE_IMAGE:
            render_image(canvas, &bounding_box, &render_command->renderData.image);
            break;
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
            render_scissor_start(canvas, &bounding_box);
            break;
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
            render_scissor_reset(canvas);
            break;
        case CLAY_RENDER_COMMAND_TYPE_CUSTOM:
            furi_crash("Custom render commands are not supported");
            break;
        }
    }
}

Clay_Dimensions clay_render_measure_text(Clay_StringSlice text, Clay_TextElementConfig* config, void* userData) {
    UNUSED(userData);
    furi_check(config->fontId < FontMax);
    U8G2FontRender* font_render = &render_font_renderers[config->fontId];

    Clay_Dimensions dimensions = {
        .width = u8g2_font_render_get_string_width_multiline(font_render, text.chars, text.length),
        .height = u8g2_font_render_get_height(font_render),
    };

    return dimensions;
}

void canvas_init(void) {
    for(int i = 0; i < FontMax; i++) {
        const void* font = render_get_font_by_id(i);
        render_font_renderers[i] = u8g2_font_render_init(font, render_draw_pixel_fg, NULL);
    }
}

static Canvas* canvas_alloc_in_place_internal(void* buffer, size_t width, size_t height, bool allocated_by_malloc) {
    Canvas* canvas = buffer;
    canvas->canary_pre = (uint32_t*)((uint8_t*)buffer + sizeof(Canvas));
    canvas->data = (Color*)(canvas->canary_pre + 1);
    memset(canvas->data, 0xFF, sizeof(Color) * width * height);
    canvas->canary_post = (uint32_t*)(canvas->data + width * height);
    *(canvas->canary_pre) = CANARY_VALUE;
    *(canvas->canary_post) = CANARY_VALUE;
    canvas->width = width;
    canvas->height = height;
    render_scissor_reset(canvas);
    canvas->allocated_by_malloc = allocated_by_malloc;

    return canvas;
}

Canvas* canvas_alloc(size_t width, size_t height) {
    void* buffer = malloc(canvas_get_required_buffer_size(width, height));
    return canvas_alloc_in_place_internal(buffer, width, height, true);
}

Canvas* canvas_alloc_in_place(void* buffer, size_t width, size_t height) {
    return canvas_alloc_in_place_internal(buffer, width, height, false);
}

size_t canvas_get_required_buffer_size(size_t width, size_t height) {
    // Canvas + canary_pre + data + canary_post
    size_t total_size = sizeof(Canvas) + sizeof(uint32_t) + sizeof(Color) * width * height + sizeof(uint32_t);
    return total_size;
}

void canvas_free(Canvas* canvas) {
    if(canvas->allocated_by_malloc) {
        free(canvas);
    }
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

Image canvas_to_image(Canvas* canvas) {
    Image image = {
        .width = canvas->width,
        .height = canvas->height,
        .format = ImageFormatRawGray8,
        .data = (uint8_t*)canvas->data,
    };
    return image;
}
