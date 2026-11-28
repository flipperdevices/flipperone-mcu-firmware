#pragma once
#include "clay.h"
#include "image.h"

typedef enum {
    FontBody, // haxrcorp4090 FlipCTL
    FontButton, // helvB08
    FontKeyboard, // profont11
    FontBusy9, // busy9
    FontBig, // Born2bSportyV2

    // Special value, used to determine the size of the Font enum. Do not use.
    FontMax,
} Font;

typedef struct Canvas Canvas;

typedef uint8_t Color;

typedef struct {
    Color color;
    uint8_t alpha;
} ColorA;

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

/** Draw an image unscaled with its top left corner at (x, y), clipped to the
 * scissors. mirror_x flips it horizontally in place; alpha below 255 blends it
 * over the canvas (0 draws nothing). Gray8 and Mono1, with or without a mask. */
void canvas_draw_image(Canvas* canvas, int32_t x, int32_t y, const Image* image, bool mirror_x, uint8_t alpha);

/** The clock (furi_get_tick(), ms) the current render pass is drawn at: every
 * element of a pass that depends on time samples this one value. */
uint32_t canvas_get_render_time(Canvas* canvas);

/** Ask for another render pass in delay_ms. The shortest request of a pass
 * wins; the GUI arms its redraw timer with it once the pass is drawn, so the
 * next pass comes that much after the end of this one. */
void canvas_request_redraw(Canvas* canvas, uint32_t delay_ms);

// rendering primitives

void render_draw_line(Canvas* canvas, int32_t x0, int32_t y0, int32_t x1, int32_t y1, ColorA color);

void render_fill_round_rectangle(Canvas* canvas, int32_t x, int32_t y, int32_t width, int32_t height, int32_t radius, ColorA color);

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
    ColorA color);

void render_draw_round_rectangle(Canvas* canvas, int32_t x, int32_t y, int32_t width, int32_t height, int32_t radius, int32_t border_width, ColorA color);

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
    ColorA color);

// clay rendering API

Clay_Dimensions clay_render_measure_text(Clay_StringSlice text, Clay_TextElementConfig* config, void* userData);

void clay_render_do_render(Canvas* canvas, Clay_RenderCommandArray* renderCommands);

/** Whether the last clay_render_do_render() pass asked for another one; if so
 * delay_ms gets the shortest delay asked. */
bool canvas_get_redraw_delay(Canvas* canvas, uint32_t* delay_ms);

/** A Clay custom element: pass its address as `.custom = {.customData = ...}`.
 * The renderer calls `render` with the scissors narrowed to the element's box;
 * it draws with the canvas_* functions and calls canvas_request_redraw() to
 * animate. The struct must outlive the render pass. */
typedef void (*ClayCustomRenderCallback)(Canvas* canvas, const Clay_BoundingBox* bounding_box, void* context);

typedef struct {
    ClayCustomRenderCallback render;
    void* context;
} ClayCustomElement;
