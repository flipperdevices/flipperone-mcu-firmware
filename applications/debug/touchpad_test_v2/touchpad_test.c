#include <furi.h>
#include <gui/gui.h>
#include <gui/clay_helper.h>
#include <m-array.h>
#include <m-algo.h>
#include <input_touch/input_touch.h>
#include "touchpad_test.h"

// Set to 1 to additionally clamp mapped touch points to the capsule outline
// (so touches near the rounded ends can't land in the corners of its
// bounding box); 0 to only map coordinates into the oval's bounding box.
#define TOUCHPAD_CLAMP_TO_OVAL (0)

// Show a marker outline at every diamond's center hit-zone; purely a debug
// visualization, the gray-fill-on-hit behavior below is always active.
#define TOUCHPAD_SHOW_HIT_ZONES (1)

// Set to 1 to make every new touch-down act like pressing "Clear" first, so
// the test restarts from scratch on each touch; 0 for the track to keep
// accumulating across touches (only the "Clear"/5 key resets it).
#define TOUCHPAD_RESET_ON_TOUCH (0)

// Caps how often the canvas is actually re-rendered and pushed to the GUI.
// Without this, every touch/input event redraws immediately, which can run
// as fast as the input source delivers events (~80 FPS observed); a periodic
// timer decouples rendering from input rate instead.
#define TOUCHPAD_TARGET_FPS (60)
#define TOUCHPAD_FRAME_INTERVAL_MS (1000 / TOUCHPAD_TARGET_FPS)

#if TOUCHPAD_CLAMP_TO_OVAL
#include <math.h>
#endif

#define TAG "TouchpadTest"

#define TOUCHPAD_CANVAS_WIDTH  (256)
#define TOUCHPAD_HEADER_HEIGHT (22)
#define TOUCHPAD_CANVAS_HEIGHT (144 - TOUCHPAD_HEADER_HEIGHT)

// Capsule outline: width/height chosen as a 3:2 ratio so that, together with
// TOUCHPAD_GRID_PITCH below, the diagonal grid tiles into an exact 6x4 grid
// of diamonds, matching the reference mock-up.
#define TOUCHPAD_OVAL_WIDTH    (180)
#define TOUCHPAD_OVAL_HEIGHT   (120)
// Shifted 15px left of center (30px left, then shifted back 15px right) to
// leave room for the reset button in the bottom-right corner.
#define TOUCHPAD_OVAL_MARGIN_X (((TOUCHPAD_CANVAS_WIDTH - TOUCHPAD_OVAL_WIDTH) / 2) - 15)
#define TOUCHPAD_OVAL_MARGIN_Y ((TOUCHPAD_CANVAS_HEIGHT - TOUCHPAD_OVAL_HEIGHT) / 2)
#define TOUCHPAD_OVAL_RADIUS   (TOUCHPAD_OVAL_HEIGHT / 2)
#define TOUCHPAD_GRID_PITCH    (TOUCHPAD_OVAL_HEIGHT / 4)

// Lattice coordinates (u = x - y, v = x + y): grid lines sit at u/v = LATTICE_U0/V0
// + n * TOUCHPAD_GRID_PITCH, and a lattice vertex is anchored exactly at the
// capsule's center. Kept as macros (rather than locals) so both the frame
// renderer and the touch/hit-testing code agree on the same lattice.
#define TOUCHPAD_BOX_X0    (TOUCHPAD_OVAL_MARGIN_X)
#define TOUCHPAD_BOX_Y0    (TOUCHPAD_OVAL_MARGIN_Y)
#define TOUCHPAD_BOX_X1    (TOUCHPAD_OVAL_MARGIN_X + TOUCHPAD_OVAL_WIDTH)
#define TOUCHPAD_BOX_Y1    (TOUCHPAD_OVAL_MARGIN_Y + TOUCHPAD_OVAL_HEIGHT)
#define TOUCHPAD_BOX_CX    ((TOUCHPAD_BOX_X0 + TOUCHPAD_BOX_X1) / 2)
#define TOUCHPAD_BOX_CY    ((TOUCHPAD_BOX_Y0 + TOUCHPAD_BOX_Y1) / 2)
#define TOUCHPAD_LATTICE_U0 (TOUCHPAD_BOX_CX - TOUCHPAD_BOX_CY)
#define TOUCHPAD_LATTICE_V0 (TOUCHPAD_BOX_CX + TOUCHPAD_BOX_CY)
// generous bound on |m|, |k| covering the whole capsule bounding box
#define TOUCHPAD_LATTICE_RANGE (6)

// Center hit-zone per diamond: a square with area ~20% of the diamond's area
// (diamond area = pitch^2 / 2 for this isotropic 45-degree grid).
// side = sqrt(0.20 * pitch^2 / 2) = pitch * sqrt(0.10) ~= pitch * 0.3162;
// for TOUCHPAD_GRID_PITCH == 30 that works out to ~9.5, rounded to 9.
#define TOUCHPAD_HIT_ZONE_SIZE (12)
#define TOUCHPAD_MAX_HIT_DIAMONDS (64)

typedef struct TouchpadTestLine {
    int32_t x0;
    int32_t y0;
    int32_t x1;
    int32_t y1;
} TouchpadTestLine;

#define TOUCHPAD_MAX_LINES_COUNT (16 * 1024 / sizeof(TouchpadTestLine))

ARRAY_DEF(TouchpadTestLineArray, TouchpadTestLine, M_POD_OPLIST);
#define M_OPL_TouchpadTestLineArray_t() ARRAY_OPLIST(TouchpadTestLineArray, M_POD_OPLIST)
ALGO_DEF(TouchpadTestLineArray, TouchpadTestLineArray_t);

typedef struct {
    Canvas* canvas;
    Image image;
    TouchpadTestLineArray_t lines;

    int32_t last_x;
    int32_t last_y;
    float pressure;
    bool pressed;

    // centers of diamonds whose center hit-zone the track has crossed
    int32_t hit_diamond_cx[TOUCHPAD_MAX_HIT_DIAMONDS];
    int32_t hit_diamond_cy[TOUCHPAD_MAX_HIT_DIAMONDS];
    size_t hit_diamond_count;
    size_t total_diamond_count; // computed once at alloc; for the "all filled" event

    bool reset_pressed; // for the on-screen reset button's pressed/highlight state

    // set whenever something changes that needs a redraw; the periodic timer
    // clears it after rendering. Without this, the redraw timer would rewrite
    // the canvas buffer unconditionally on every tick even while idle, which
    // fights with the display's own refresh and tears the frame.
    bool dirty;

    FuriPubSub* event_pubsub; // not owned; copy of TouchpadTestApp's, so model-side
                              // code (e.g. touchpad_test_v2_app_model_push_line) can publish

} TouchpadTestModel;

typedef struct {
    Gui* gui;
    View* view;
    FuriEventLoop* event_loop;
    FuriThread* thread;
    FuriPubSub* event_pubsub;
    FuriEventLoopTimer* redraw_timer;
} TouchpadTestApp;

/**
 * @brief Whether (x, y) lies within the capsule outline (a rounded rectangle
 * whose radius equals half its height, i.e. two semicircular ends joined by
 * a straight midsection).
 */
static bool touchpad_test_v2_oval_contains(int32_t x, int32_t y) {
    int32_t left_cx = TOUCHPAD_OVAL_MARGIN_X + TOUCHPAD_OVAL_RADIUS;
    int32_t right_cx = TOUCHPAD_OVAL_MARGIN_X + TOUCHPAD_OVAL_WIDTH - TOUCHPAD_OVAL_RADIUS;
    int32_t cy = TOUCHPAD_OVAL_MARGIN_Y + TOUCHPAD_OVAL_RADIUS;

    int32_t cx;
    if(x < left_cx) {
        cx = left_cx;
    } else if(x > right_cx) {
        cx = right_cx;
    } else {
        // straight midsection: inside iff y is within the capsule's vertical extent
        return y >= TOUCHPAD_OVAL_MARGIN_Y && y <= TOUCHPAD_OVAL_MARGIN_Y + TOUCHPAD_OVAL_HEIGHT;
    }

    int32_t dx = x - cx;
    int32_t dy = y - cy;
    return (dx * dx + dy * dy) <= (TOUCHPAD_OVAL_RADIUS * TOUCHPAD_OVAL_RADIUS);
}

#if TOUCHPAD_CLAMP_TO_OVAL
/**
 * @brief Clamps (x, y) to the capsule outline. The capsule is convex, so a
 * line segment between two clamped points never leaves it either.
 */
static void touchpad_test_v2_clamp_to_oval(int32_t* x, int32_t* y) {
    int32_t left_cx = TOUCHPAD_OVAL_MARGIN_X + TOUCHPAD_OVAL_RADIUS;
    int32_t right_cx = TOUCHPAD_OVAL_MARGIN_X + TOUCHPAD_OVAL_WIDTH - TOUCHPAD_OVAL_RADIUS;
    int32_t cy = TOUCHPAD_OVAL_MARGIN_Y + TOUCHPAD_OVAL_RADIUS;

    int32_t cx;
    if(*x < left_cx) {
        cx = left_cx;
    } else if(*x > right_cx) {
        cx = right_cx;
    } else {
        // straight midsection: just clamp the vertical extent
        if(*y < TOUCHPAD_OVAL_MARGIN_Y) *y = TOUCHPAD_OVAL_MARGIN_Y;
        if(*y > TOUCHPAD_OVAL_MARGIN_Y + TOUCHPAD_OVAL_HEIGHT) *y = TOUCHPAD_OVAL_MARGIN_Y + TOUCHPAD_OVAL_HEIGHT;
        return;
    }

    int32_t dx = *x - cx;
    int32_t dy = *y - cy;
    int32_t dist_sq = dx * dx + dy * dy;
    if(dist_sq <= TOUCHPAD_OVAL_RADIUS * TOUCHPAD_OVAL_RADIUS) return;

    float dist = sqrtf((float)dist_sq);
    float scale = (float)TOUCHPAD_OVAL_RADIUS / dist;
    *x = cx + (int32_t)((float)dx * scale);
    *y = cy + (int32_t)((float)dy * scale);
}
#endif

/** Floor division, assuming a positive divisor (unlike C's truncating '/'). */
static int32_t touchpad_test_v2_floor_div(int32_t a, int32_t b) {
    int32_t q = a / b;
    int32_t r = a % b;
    if(r != 0 && r < 0) q--;
    return q;
}

/** Center, in canvas coordinates, of the diamond at lattice indices (m, k). */
static void touchpad_test_v2_diamond_center(int32_t m, int32_t k, int32_t* cx, int32_t* cy) {
    int32_t c1 = TOUCHPAD_LATTICE_U0 + m * TOUCHPAD_GRID_PITCH;
    int32_t c2 = TOUCHPAD_LATTICE_V0 + k * TOUCHPAD_GRID_PITCH;
    *cx = (c1 + c2 + TOUCHPAD_GRID_PITCH) / 2;
    *cy = (c2 - c1) / 2;
}

/**
 * @brief Whether at least one pixel of the diamond centered at (cx, cy)'s
 * center hit-zone lies within the true capsule outline. This is the single
 * source of truth for which diamonds "exist": touchpad_test_v2_count_total_diamonds()
 * uses it to build the total, and touchpad_test_v2_app_model_push_line() uses
 * it to reject hits on cells outside the capsule (touches aren't clamped to
 * the oval, so a drawn segment can otherwise reach a hit-zone that's
 * entirely off the capsule - without this check, that inflates
 * hit_diamond_count past total_diamond_count without covering every real
 * diamond, so "all filled" could fire early).
 */
static bool touchpad_test_v2_diamond_is_reachable(int32_t cx, int32_t cy) {
    const int32_t half = TOUCHPAD_HIT_ZONE_SIZE / 2;
    for(int32_t dy = -half; dy <= half; dy++) {
        for(int32_t dx = -half; dx <= half; dx++) {
            if(touchpad_test_v2_oval_contains(cx + dx, cy + dy)) return true;
        }
    }
    return false;
}

/**
 * @brief Counts how many diamonds are actually reachable by touch (see
 * touchpad_test_v2_diamond_is_reachable()). Used to detect when every
 * diamond has been filled at least once.
 */
static size_t touchpad_test_v2_count_total_diamonds(void) {
    size_t count = 0;

    for(int32_t m = -TOUCHPAD_LATTICE_RANGE; m <= TOUCHPAD_LATTICE_RANGE; m++) {
        for(int32_t k = -TOUCHPAD_LATTICE_RANGE; k <= TOUCHPAD_LATTICE_RANGE; k++) {
            int32_t cx, cy;
            touchpad_test_v2_diamond_center(m, k, &cx, &cy);
            if(cx < TOUCHPAD_BOX_X0 - TOUCHPAD_GRID_PITCH || cx > TOUCHPAD_BOX_X1 + TOUCHPAD_GRID_PITCH) continue;
            if(cy < TOUCHPAD_BOX_Y0 - TOUCHPAD_GRID_PITCH || cy > TOUCHPAD_BOX_Y1 + TOUCHPAD_GRID_PITCH) continue;

            if(touchpad_test_v2_diamond_is_reachable(cx, cy)) count++;
        }
    }

    return count;
}

/** Whether the segment (x0, y0)-(x1, y1) intersects the axis-aligned box [bx0, bx1] x [by0, by1]. */
static bool touchpad_test_v2_segment_intersects_box(
    int32_t x0,
    int32_t y0,
    int32_t x1,
    int32_t y1,
    int32_t bx0,
    int32_t by0,
    int32_t bx1,
    int32_t by1) {
    // Liang-Barsky line clipping, used here purely as an intersection test
    float t0 = 0.0f, t1 = 1.0f;
    float dx = (float)(x1 - x0);
    float dy = (float)(y1 - y0);
    float p[4] = {-dx, dx, -dy, dy};
    float q[4] = {
        (float)(x0 - bx0),
        (float)(bx1 - x0),
        (float)(y0 - by0),
        (float)(by1 - y0),
    };
    for(int i = 0; i < 4; i++) {
        if(p[i] == 0.0f) {
            if(q[i] < 0.0f) return false; // parallel to this edge and outside it
        } else {
            float r = q[i] / p[i];
            if(p[i] < 0.0f) {
                if(r > t1) return false;
                if(r > t0) t0 = r;
            } else {
                if(r < t0) return false;
                if(r < t1) t1 = r;
            }
        }
    }
    return t0 <= t1;
}

/** Fills the diamond (rhombus) centered at (cx, cy) with the given half-size (tip-to-center distance). */
static void touchpad_test_v2_fill_diamond(Canvas* canvas, int32_t cx, int32_t cy, int32_t half_size, ColorA color) {
    for(int32_t dy = -half_size; dy <= half_size; dy++) {
        int32_t half_width = half_size - (dy < 0 ? -dy : dy);
        render_draw_line(canvas, cx - half_width, cy + dy, cx + half_width, cy + dy, color);
    }
}

#if TOUCHPAD_SHOW_HIT_ZONES
/** Draws the outline of a diamond's center hit-zone, for debugging. */
static void touchpad_test_v2_draw_zone_marker(Canvas* canvas, int32_t cx, int32_t cy, ColorA color) {
    int32_t half = TOUCHPAD_HIT_ZONE_SIZE / 2;
    render_draw_line(canvas, cx - half, cy - half, cx + half, cy - half, color);
    render_draw_line(canvas, cx - half, cy + half, cx + half, cy + half, color);
    render_draw_line(canvas, cx - half, cy - half, cx - half, cy + half, color);
    render_draw_line(canvas, cx + half, cy - half, cx + half, cy + half, color);
}
#endif

/** Draws the portion of the 45-degree line (x - y = c) that lies within [x0, x1] x [y0, y1]. */
static void touchpad_test_v2_draw_diag_backslash(
    Canvas* canvas,
    int32_t c,
    int32_t x0,
    int32_t y0,
    int32_t x1,
    int32_t y1,
    ColorA color) {
    int32_t xs = MAX(x0, c + y0);
    int32_t xe = MIN(x1, c + y1);
    if(xs > xe) return;
    render_draw_line(canvas, xs, xs - c, xe, xe - c, color);
}

/** Draws the portion of the 45-degree line (x + y = c) that lies within [x0, x1] x [y0, y1]. */
static void touchpad_test_v2_draw_diag_slash(
    Canvas* canvas,
    int32_t c,
    int32_t x0,
    int32_t y0,
    int32_t x1,
    int32_t y1,
    ColorA color) {
    int32_t xs = MAX(x0, c - y1);
    int32_t xe = MIN(x1, c - y0);
    if(xs > xe) return;
    render_draw_line(canvas, xs, c - xs, xe, c - xe, color);
}

/** Button styled the same way as keypad_test.c's keypad buttons. */
static void touchpad_test_v2_app_create_reset_button(Clay_ElementId id, Clay_String text, bool inverted) {
    CLAY(
        id,
        {
            .border = {.color = COLOR_BLACK, .width = {.top = 1, .left = 1, .right = 1, .bottom = 1}},
            .layout =
                {
                    .padding = {8, 8, 4, 4},
                    .sizing = {.width = CLAY_SIZING_FIXED(40)},
                    .childAlignment = {.x = CLAY_ALIGN_X_CENTER},
                },
            .backgroundColor = inverted ? COLOR_WHITE : COLOR_BLACK,
            .cornerRadius = CLAY_CORNER_RADIUS(4),
        }) {
        CLAY_TEXT(text, CLAY_TEXT_CONFIG({.fontId = FontButton, .textColor = inverted ? COLOR_BLACK : COLOR_WHITE}));
    }
}

static bool touchpad_test_v2_app_layout(void* _model) {
    furi_assert(_model);
    TouchpadTestModel* model = _model;

    Clay_Sizing layoutExpand = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
    Clay_BorderElementConfig contentBorders = {.color = COLOR_BLACK, .width = {.top = 1, .left = 1, .right = 1, .bottom = 1}};

    CLAY(
        CLAY_APP_ID("OuterContainer"),
        {.backgroundColor = COLOR_WHITE,
         .layout = {
             .layoutDirection = CLAY_TOP_TO_BOTTOM,
             .sizing = layoutExpand,
         }}) {
        CLAY(
            CLAY_APP_ID("Header"),
            {
                .layout =
                    {
                        .sizing = {.height = CLAY_SIZING_FIXED(TOUCHPAD_HEADER_HEIGHT), .width = CLAY_SIZING_GROW(0)},
                        .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                    },
            }) {
            CLAY(CLAY_APP_ID("HeaderText"), {.layout = {.padding = {8, 8, 4, 4}}}) {
                CLAY_TEXT(CLAY_STRING("Touchpad Test"), CLAY_TEXT_CONFIG({.fontId = FontButton, .textColor = COLOR_BLACK}));
            }
            CLAY(
                CLAY_APP_ID("ClearHint"),
                {
                    .layout =
                        {
                            .padding = {8, 8, 4, 4},
                        },
                    .floating =
                        {
                            .attachPoints = {.element = CLAY_ATTACH_POINT_RIGHT_CENTER, .parent = CLAY_ATTACH_POINT_RIGHT_CENTER},
                            .attachTo = CLAY_ATTACH_TO_PARENT,
                        },
                }) {
                CLAY_TEXT(
                    clay_helper_string_from_chars(
                        (model->total_diamond_count > 0 && model->hit_diamond_count >= model->total_diamond_count) ?
                            "test OK" :
                            "testing"),
                    CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = COLOR_BLACK}));
            }
        }
        CLAY(
            CLAY_APP_ID("MainContent"),
            {
                .clip = {.vertical = true},
                .layout =
                    {
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        .sizing = layoutExpand,
                        .childAlignment = {.y = CLAY_ALIGN_Y_TOP, .x = CLAY_ALIGN_X_LEFT},
                    },
                .image = {.imageData = &model->image},
            }) {
            CLAY(
                CLAY_APP_ID("ResetButtonSlot"),
                {
                    .layout =
                        {
                            .padding = {8, 8, 8, 8},
                        },
                    .floating =
                        {
                            .attachPoints = {.element = CLAY_ATTACH_POINT_RIGHT_BOTTOM, .parent = CLAY_ATTACH_POINT_RIGHT_BOTTOM},
                            .attachTo = CLAY_ATTACH_TO_PARENT,
                        },
                }) {
                touchpad_test_v2_app_create_reset_button(CLAY_APP_ID("ResetButton"), CLAY_STRING("Clear"), model->reset_pressed);
            }
        }
    }

    return false;
}

void touchpad_test_v2_app_update_frame(TouchpadTestModel* model) {
    canvas_clear(model->canvas, 0xFF);

    ColorA color_gray = {.color = 160, .alpha = 255};
    ColorA color_black = {.color = 0x00, .alpha = 255};

    // diagonal (45-degree) diamond grid, clipped to the capsule's bounding box
    const int32_t box_x0 = TOUCHPAD_BOX_X0;
    const int32_t box_y0 = TOUCHPAD_BOX_Y0;
    const int32_t box_x1 = TOUCHPAD_BOX_X1;
    const int32_t box_y1 = TOUCHPAD_BOX_Y1;

    // anchor a lattice vertex exactly at the capsule's center
    const int32_t c_backslash0 = TOUCHPAD_LATTICE_U0;
    const int32_t c_slash0 = TOUCHPAD_LATTICE_V0;
    const int32_t n_max = (box_x1 - box_x0 + box_y1 - box_y0) / TOUCHPAD_GRID_PITCH + 1;

#if TOUCHPAD_SHOW_HIT_ZONES
    // debug: mark every diamond's center hit-zone. Drawn before the fill
    // below so a filled diamond's marker gets painted over (hidden), while
    // still drawn before the grid lines so those remain visible on top of
    // the (lighter) fill instead of being painted over.
    for(int32_t m = -TOUCHPAD_LATTICE_RANGE; m <= TOUCHPAD_LATTICE_RANGE; m++) {
        for(int32_t k = -TOUCHPAD_LATTICE_RANGE; k <= TOUCHPAD_LATTICE_RANGE; k++) {
            int32_t cx, cy;
            touchpad_test_v2_diamond_center(m, k, &cx, &cy);
            // loose reject, purely to skip cells far outside the visible area;
            // the full-canvas mask below cleans up any remaining overshoot
            if(cx < box_x0 - TOUCHPAD_GRID_PITCH || cx > box_x1 + TOUCHPAD_GRID_PITCH) continue;
            if(cy < box_y0 - TOUCHPAD_GRID_PITCH || cy > box_y1 + TOUCHPAD_GRID_PITCH) continue;
            touchpad_test_v2_draw_zone_marker(model->canvas, cx, cy, color_black);
        }
    }
#endif

    // fill diamonds whose center hit-zone the track has crossed
    ColorA color_hit = {.color = 235, .alpha = 255};
    for(size_t i = 0; i < model->hit_diamond_count; i++) {
        touchpad_test_v2_fill_diamond(
            model->canvas, model->hit_diamond_cx[i], model->hit_diamond_cy[i], TOUCHPAD_GRID_PITCH / 2, color_hit);
    }

    // grid lines drawn last (of these three) so they stay visible on top of
    // the fill instead of being painted over
    for(int32_t n = -n_max; n <= n_max; n++) {
        touchpad_test_v2_draw_diag_backslash(
            model->canvas, c_backslash0 + n * TOUCHPAD_GRID_PITCH, box_x0, box_y0, box_x1, box_y1, color_gray);
        touchpad_test_v2_draw_diag_slash(
            model->canvas, c_slash0 + n * TOUCHPAD_GRID_PITCH, box_x0, box_y0, box_x1, box_y1, color_gray);
    }

    // the grid/fills above were only clipped to the bounding box (or not at
    // all, for edge diamonds whose fill spills past it); mask the whole
    // canvas back to the background color outside the true capsule outline,
    // so anything drawn anywhere gets cleaned up regardless of how far it
    // overshoots
    Color* data = canvas_get_data(model->canvas);
    size_t canvas_width = canvas_get_width(model->canvas);
    size_t canvas_height = canvas_get_height(model->canvas);
    for(size_t y = 0; y < canvas_height; y++) {
        for(size_t x = 0; x < canvas_width; x++) {
            if(!touchpad_test_v2_oval_contains((int32_t)x, (int32_t)y)) data[y * canvas_width + x] = 0xFF;
        }
    }

    // capsule outline (drawn last so the mask above doesn't eat into it);
    // drawn bolder once every diamond has been filled ("test ok"). The
    // border_width param to render_draw_round_rectangle only thickens the
    // straight top/bottom edges - its rounded corners are drawn via
    // render_draw_arc, which always renders 1px regardless of border_width.
    // So for a uniformly thick outline we instead stack several 1px-wide
    // capsules, each inset by one more pixel (radius shrunk to match).
    bool test_ok = model->total_diamond_count > 0 && model->hit_diamond_count >= model->total_diamond_count;
    const int32_t border_thickness = test_ok ? 3 : 1;
    for(int32_t i = 0; i < border_thickness; i++) {
        int32_t w = TOUCHPAD_OVAL_WIDTH - 2 * i;
        int32_t h = TOUCHPAD_OVAL_HEIGHT - 2 * i;
        render_draw_round_rectangle(
            model->canvas, TOUCHPAD_OVAL_MARGIN_X + i, TOUCHPAD_OVAL_MARGIN_Y + i, w, h, h / 2, 1, color_black);
    }

    // touch lines
    for(size_t i = 0; i < TouchpadTestLineArray_size(model->lines); i++) {
        TouchpadTestLine* line = TouchpadTestLineArray_get(model->lines, i);
        render_draw_line(model->canvas, line->x0, line->y0, line->x1, line->y1, color_black);
    }

    // touch point
    const int32_t min_radius = 3;
    const int32_t radius = min_radius + (int32_t)(model->pressure * 20);
    if(model->pressed) {
        render_fill_round_rectangle(model->canvas, model->last_x - radius, model->last_y - radius, 2 * radius, 2 * radius, radius, color_black);
    } else {
        render_draw_round_rectangle(model->canvas, model->last_x - radius, model->last_y - radius, 2 * radius, 2 * radius, radius, 1, color_black);
    }
}

/**
 * @brief Periodic redraw tick (at most TOUCHPAD_TARGET_FPS): input handlers
 * only update model state and set model->dirty; this is the only place that
 * actually re-renders the canvas and pushes it to the GUI, so the redraw
 * rate stays capped instead of running once per input event (as fast as
 * ~80/s from the touchscreen). It skips the render entirely when nothing is
 * dirty, so an idle screen isn't rewritten every tick for no reason - doing
 * that constantly fought with the display's own refresh and tore the frame.
 */
static void touchpad_test_v2_app_redraw_timer_callback(void* context) {
    furi_assert(context);
    TouchpadTestApp* instance = context;
    bool needs_render = false;
    with_view_model(
        instance->view,
        TouchpadTestModel * model,
        {
            needs_render = model->dirty;
            if(needs_render) {
                touchpad_test_v2_app_update_frame(model);
                model->dirty = false;
            }
        },
        needs_render);
}

static bool touchpad_test_v2_app_input(InputEvent* event, void* context) {
    furi_check(context);
    TouchpadTestApp* instance = context;
    bool consumed = false;

    if(event->type == InputTypePress) {
        if(event->key == InputKeyBack) {
            furi_thread_signal(instance->thread, FuriSignalExit, NULL);
            consumed = true;
        } else if(event->key == InputKey5) {
            with_view_model(
                instance->view,
                TouchpadTestModel * model,
                {
                    TouchpadTestLineArray_reset(model->lines);
                    model->hit_diamond_count = 0;
                    model->reset_pressed = true;
                    model->dirty = true;
                },
                false);
            TouchpadTestEvent evt = {.tp_status = TouchpadTestStatusCleared};
            furi_pubsub_publish(instance->event_pubsub, &evt);
            consumed = true;
        }
    } else if(event->type == InputTypeRelease) {
        if(event->key == InputKey5) {
            with_view_model(
                instance->view,
                TouchpadTestModel * model,
                {
                    model->reset_pressed = false;
                    model->dirty = true;
                },
                false);
            consumed = true;
        }
    }

    return consumed;
}

static void touchpad_test_v2_app_model_push_line(TouchpadTestModel* model, int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    TouchpadTestLine line = {.x0 = x0, .y0 = y0, .x1 = x1, .y1 = y1};
    TouchpadTestLineArray_push_back(model->lines, line);

    while(TouchpadTestLineArray_size(model->lines) > TOUCHPAD_MAX_LINES_COUNT) {
        TouchpadTestLineArray_pop_at(NULL, model->lines, 0);
    }

    // check which diamonds' center hit-zones this new segment crosses, over
    // the rectangular range of lattice cells spanned by its two endpoints
    int32_t m0 = touchpad_test_v2_floor_div(x0 - y0 - TOUCHPAD_LATTICE_U0, TOUCHPAD_GRID_PITCH);
    int32_t m1 = touchpad_test_v2_floor_div(x1 - y1 - TOUCHPAD_LATTICE_U0, TOUCHPAD_GRID_PITCH);
    int32_t k0 = touchpad_test_v2_floor_div(x0 + y0 - TOUCHPAD_LATTICE_V0, TOUCHPAD_GRID_PITCH);
    int32_t k1 = touchpad_test_v2_floor_div(x1 + y1 - TOUCHPAD_LATTICE_V0, TOUCHPAD_GRID_PITCH);
    int32_t m_lo = MIN(m0, m1), m_hi = MAX(m0, m1);
    int32_t k_lo = MIN(k0, k1), k_hi = MAX(k0, k1);

    for(int32_t m = m_lo; m <= m_hi; m++) {
        for(int32_t k = k_lo; k <= k_hi; k++) {
            int32_t cx, cy;
            touchpad_test_v2_diamond_center(m, k, &cx, &cy);

            // edge diamonds (center on/beyond the bounding box) are allowed
            // to register a hit too: touchpad_test_v2_app_update_frame now
            // masks the whole canvas against the true capsule outline, so
            // their fill is cleaned up correctly even if it spills past the
            // box on the side that's outside the capsule. But reject cells
            // that aren't reachable at all (touches aren't clamped to the
            // oval, so a segment can reach a hit-zone that's entirely off
            // the capsule) - otherwise those inflate hit_diamond_count
            // without matching anything counted in total_diamond_count,
            // letting "all filled" fire before every real diamond is hit.
            if(!touchpad_test_v2_diamond_is_reachable(cx, cy)) continue;

            const int32_t half = TOUCHPAD_HIT_ZONE_SIZE / 2;
            if(!touchpad_test_v2_segment_intersects_box(
                   x0, y0, x1, y1, cx - half, cy - half, cx + half, cy + half))
                continue;

            bool already_hit = false;
            for(size_t i = 0; i < model->hit_diamond_count; i++) {
                if(model->hit_diamond_cx[i] == cx && model->hit_diamond_cy[i] == cy) {
                    already_hit = true;
                    break;
                }
            }
            if(!already_hit && model->hit_diamond_count < TOUCHPAD_MAX_HIT_DIAMONDS) {
                model->hit_diamond_cx[model->hit_diamond_count] = cx;
                model->hit_diamond_cy[model->hit_diamond_count] = cy;
                model->hit_diamond_count++;

                TouchpadTestEvent evt = {
                    .tp_status = TouchpadTestStatusDiamondFilled,
                    .diamond_x = cx,
                    .diamond_y = cy,
                };
                furi_pubsub_publish(model->event_pubsub, &evt);

                if(model->hit_diamond_count == model->total_diamond_count) {
                    TouchpadTestEvent all_filled_evt = {.tp_status = TouchpadTestStatusAllFilled};
                    furi_pubsub_publish(model->event_pubsub, &all_filled_evt);
                }
            }
        }
    }
}

static bool touchpad_test_v2_app_input_touch(InputTouchEvent* event, void* context) {
    furi_check(context);
    TouchpadTestApp* instance = context;
    bool consumed = false;
    float scale_x = 0.7f;
    float scale_y = 0.6f;
    const int32_t touch_real_resolution_x = TOUCHPAD_RESOLUTION_X;
    const int32_t touch_real_resolution_y = TOUCHPAD_RESOLUTION_Y;
    const int32_t touch_resolution_x = touch_real_resolution_x * scale_x;
    const int32_t touch_resolution_y = touch_real_resolution_y * scale_y;
    const int32_t touch_resolution_padding_x = (touch_real_resolution_x - touch_resolution_x) / 2;
    const int32_t touch_resolution_padding_y = (touch_real_resolution_y - touch_resolution_y) / 2;

    switch(event->type) {
    case InputTouchTypeStart:
        with_view_model(
            instance->view,
            TouchpadTestModel * model,
            {
#if TOUCHPAD_RESET_ON_TOUCH
                // start a fresh track on every new touch-down, same as the "Clear" key
                TouchpadTestLineArray_reset(model->lines);
                model->hit_diamond_count = 0;
#endif
                model->pressed = true;
                model->last_x = TOUCHPAD_OVAL_MARGIN_X + (event->x - touch_resolution_padding_x) * TOUCHPAD_OVAL_WIDTH / touch_resolution_x;
                model->last_y = TOUCHPAD_OVAL_MARGIN_Y + (event->y - touch_resolution_padding_y) * TOUCHPAD_OVAL_HEIGHT / touch_resolution_y;
#if TOUCHPAD_CLAMP_TO_OVAL
                touchpad_test_v2_clamp_to_oval(&model->last_x, &model->last_y);
#endif
                model->pressure = event->pressure / TOUCHPAD_RESOLUTION_PRESSURE;
                model->dirty = true;
            },
            false);
#if TOUCHPAD_RESET_ON_TOUCH
        {
            TouchpadTestEvent evt = {.tp_status = TouchpadTestStatusCleared};
            furi_pubsub_publish(instance->event_pubsub, &evt);
        }
#endif
        consumed = true;
        break;
    case InputTouchTypeMove:
        with_view_model(
            instance->view,
            TouchpadTestModel * model,
            {
                int32_t new_x = TOUCHPAD_OVAL_MARGIN_X + (event->x - touch_resolution_padding_x) * TOUCHPAD_OVAL_WIDTH / touch_resolution_x;
                int32_t new_y = TOUCHPAD_OVAL_MARGIN_Y + (event->y - touch_resolution_padding_y) * TOUCHPAD_OVAL_HEIGHT / touch_resolution_y;
#if TOUCHPAD_CLAMP_TO_OVAL
                touchpad_test_v2_clamp_to_oval(&new_x, &new_y);
#endif
                if(model->pressed) {
                    touchpad_test_v2_app_model_push_line(model, model->last_x, model->last_y, new_x, new_y);
                }
                model->last_x = new_x;
                model->last_y = new_y;
                model->pressure = event->pressure / TOUCHPAD_RESOLUTION_PRESSURE;
                model->dirty = true;
            },
            false);
        consumed = true;
        break;
    case InputTouchTypeEnd:
        with_view_model(
            instance->view,
            TouchpadTestModel * model,
            {
                model->pressed = false;
                model->pressure = event->pressure / TOUCHPAD_RESOLUTION_PRESSURE;
                model->dirty = true;
            },
            false);
        consumed = true;
        break;
    default:
        break;
    }

    return consumed;
}

static TouchpadTestApp* touchpad_test_v2_app_alloc(void) {
    TouchpadTestApp* instance = malloc(sizeof(TouchpadTestApp));
    instance->gui = furi_record_open(RECORD_GUI);
    instance->event_loop = furi_event_loop_alloc();
    instance->thread = furi_thread_get_current();
    instance->event_pubsub = furi_pubsub_alloc();

    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLockFree, sizeof(TouchpadTestModel));

    with_view_model(
        instance->view,
        TouchpadTestModel * model,
        {
            TouchpadTestLineArray_init(model->lines);
            model->last_x = TOUCHPAD_CANVAS_WIDTH / 2;
            model->last_y = TOUCHPAD_CANVAS_HEIGHT / 2;
            model->pressed = false;
            model->hit_diamond_count = 0;
            model->total_diamond_count = touchpad_test_v2_count_total_diamonds();
            model->reset_pressed = false;
            model->event_pubsub = instance->event_pubsub;
            model->canvas = canvas_alloc(TOUCHPAD_CANVAS_WIDTH, TOUCHPAD_CANVAS_HEIGHT);
            model->image = canvas_to_image(model->canvas);
            touchpad_test_v2_app_update_frame(model);
            model->dirty = false;
        },
        false);

    view_set_layout_callback(instance->view, touchpad_test_v2_app_layout);
    view_set_input_callback(instance->view, touchpad_test_v2_app_input, instance);
    view_set_input_touch_callback(instance->view, touchpad_test_v2_app_input_touch, instance);
    gui_add_view(instance->gui, instance->view, GuiViewPriorityApplication);

    instance->redraw_timer = furi_event_loop_timer_alloc(
        instance->event_loop, touchpad_test_v2_app_redraw_timer_callback, FuriEventLoopTimerTypePeriodic, instance);
    furi_event_loop_timer_start(instance->redraw_timer, TOUCHPAD_FRAME_INTERVAL_MS);

    furi_record_create(RECORD_TOUCHPAD_TEST, instance->event_pubsub);

    TouchpadTestEvent evt = {.tp_status = TouchpadTestStatusStarted};
    furi_pubsub_publish(instance->event_pubsub, &evt);

    return instance;
}

static void touchpad_test_v2_app_free(TouchpadTestApp* instance) {
    TouchpadTestEvent ended_evt = {.tp_status = TouchpadTestStatusEnded};
    furi_pubsub_publish(instance->event_pubsub, &ended_evt);

    // Give subscribers (e.g. a CLI dump command running on its own thread) a
    // chance to react to the "Ended" event and unsubscribe + furi_record_close()
    // before we tear down the pubsub. furi_record_destroy() only succeeds once
    // nobody still holds the record open, and furi_pubsub_free() requires an
    // empty subscriber list - without this wait, both would race a subscriber
    // that hasn't had a chance to run yet and furi_check() would crash.
    bool record_destroyed = false;
    for(int i = 0; i < 100 && !record_destroyed; i++) {
        record_destroyed = furi_record_destroy(RECORD_TOUCHPAD_TEST);
        if(!record_destroyed) furi_delay_ms(10);
    }
    if(!record_destroyed) {
        FURI_LOG_W(TAG, "RECORD_TOUCHPAD_TEST still held by a subscriber; leaking its pubsub");
    }

    gui_remove_view(instance->gui, instance->view);
    furi_record_close(RECORD_GUI);
    with_view_model(
        instance->view,
        TouchpadTestModel * model,
        {
            canvas_free(model->canvas);
            TouchpadTestLineArray_clear(model->lines);
        },
        false);
    if(record_destroyed) {
        furi_pubsub_free(instance->event_pubsub);
    }
    view_free(instance->view);
    furi_event_loop_timer_free(instance->redraw_timer);
    furi_event_loop_free(instance->event_loop);
    free(instance);
}

int32_t touchpad_test_v2_app(void* p) {
    UNUSED(p);
    TouchpadTestApp* instance = touchpad_test_v2_app_alloc();
    furi_event_loop_run(instance->event_loop);
    touchpad_test_v2_app_free(instance);
    return 0;
}
