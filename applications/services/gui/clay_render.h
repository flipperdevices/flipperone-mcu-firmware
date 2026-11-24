#pragma once
#include "clay.h"
#include "image.h"

typedef enum {
    FontBody, // haxrcorp4089
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

/** Draw an image with its top-left corner at (x, y), clipped to the current
 *  scissor rectangle. Handles every ImageFormat; an animation picks its frame
 *  from the render clock and asks for the redraw that shows the next one.
 *  mirror_x flips it horizontally within the same rectangle. */
void canvas_draw_image(Canvas* canvas, int32_t x, int32_t y, const Image* image, bool mirror_x);

/** The clock (furi_get_tick(), ms) the current render pass is drawn at. Every
 *  time-dependent element samples this one value, so one pass is consistent. */
uint32_t canvas_get_render_time(Canvas* canvas);

/** Ask for another render pass in delay_ms (from the render clock). The
 *  shortest request of a pass wins; the GUI arms its redraw timer with it. */
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

/** Whether anything in the last clay_render_do_render() pass asked to be drawn
 *  again (an animated image, a custom element). If so, *delay_ms receives the
 *  shortest requested delay, measured from the clock that pass was sampled at. */
bool canvas_get_redraw_delay(Canvas* canvas, uint32_t* delay_ms);

/** A Clay custom element: pass its address as `.custom = {.customData = ...}`
 *  and the renderer calls `render` with the scissor set to the element's box.
 *  Draw with the canvas_* primitives; call canvas_request_redraw() to animate. */
typedef void (*ClayCustomRenderCallback)(Canvas* canvas, const Clay_BoundingBox* bounding_box, void* context);

typedef struct {
    ClayCustomRenderCallback render;
    void* context;
} ClayCustomElement;
