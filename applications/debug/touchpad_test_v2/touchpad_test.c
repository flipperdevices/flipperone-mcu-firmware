#include <furi.h>
#include <gui/gui.h>
#include <gui/clay_helper.h>
#include <m-array.h>
#include <m-algo.h>
#include <input_touch/input_touch.h>

// Set to 1 to additionally clamp mapped touch points to the capsule outline
// (so touches near the rounded ends can't land in the corners of its
// bounding box); 0 to only map coordinates into the oval's bounding box.
#define TOUCHPAD_CLAMP_TO_OVAL (1)

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
#define TOUCHPAD_OVAL_MARGIN_X ((TOUCHPAD_CANVAS_WIDTH - TOUCHPAD_OVAL_WIDTH) / 2)
#define TOUCHPAD_OVAL_MARGIN_Y ((TOUCHPAD_CANVAS_HEIGHT - TOUCHPAD_OVAL_HEIGHT) / 2)
#define TOUCHPAD_OVAL_RADIUS   (TOUCHPAD_OVAL_HEIGHT / 2)
#define TOUCHPAD_GRID_PITCH    (TOUCHPAD_OVAL_HEIGHT / 4)

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

} TouchpadTestModel;

typedef struct {
    Gui* gui;
    View* view;
    FuriEventLoop* event_loop;
    FuriThread* thread;
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
        return true; // straight midsection: always inside vertically here
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
                CLAY_TEXT(CLAY_STRING("Ok to clear"), CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = COLOR_BLACK}));
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
        }
    }

    return false;
}

void touchpad_test_v2_app_update_frame(TouchpadTestModel* model) {
    canvas_clear(model->canvas, 0xFF);

    ColorA color_gray = {.color = 220, .alpha = 255};
    ColorA color_black = {.color = 0x00, .alpha = 255};

    // diagonal (45-degree) diamond grid, clipped to the capsule's bounding box
    const int32_t box_x0 = TOUCHPAD_OVAL_MARGIN_X;
    const int32_t box_y0 = TOUCHPAD_OVAL_MARGIN_Y;
    const int32_t box_x1 = TOUCHPAD_OVAL_MARGIN_X + TOUCHPAD_OVAL_WIDTH;
    const int32_t box_y1 = TOUCHPAD_OVAL_MARGIN_Y + TOUCHPAD_OVAL_HEIGHT;
    const int32_t box_cx = (box_x0 + box_x1) / 2;
    const int32_t box_cy = (box_y0 + box_y1) / 2;

    // anchor a lattice vertex exactly at the capsule's center
    const int32_t c_backslash0 = box_cx - box_cy;
    const int32_t c_slash0 = box_cx + box_cy;
    const int32_t n_max = (box_x1 - box_x0 + box_y1 - box_y0) / TOUCHPAD_GRID_PITCH + 1;

    for(int32_t n = -n_max; n <= n_max; n++) {
        touchpad_test_v2_draw_diag_backslash(
            model->canvas, c_backslash0 + n * TOUCHPAD_GRID_PITCH, box_x0, box_y0, box_x1, box_y1, color_gray);
        touchpad_test_v2_draw_diag_slash(
            model->canvas, c_slash0 + n * TOUCHPAD_GRID_PITCH, box_x0, box_y0, box_x1, box_y1, color_gray);
    }

    // the grid above was only clipped to the bounding box, which is wider than
    // the capsule's rounded ends; mask the excess back to the background color
    Color* data = canvas_get_data(model->canvas);
    size_t canvas_width = canvas_get_width(model->canvas);
    int32_t left_cx = TOUCHPAD_OVAL_MARGIN_X + TOUCHPAD_OVAL_RADIUS;
    int32_t right_cx = TOUCHPAD_OVAL_MARGIN_X + TOUCHPAD_OVAL_WIDTH - TOUCHPAD_OVAL_RADIUS;
    for(int32_t y = box_y0; y < box_y1; y++) {
        for(int32_t x = box_x0; x < left_cx; x++) {
            if(!touchpad_test_v2_oval_contains(x, y)) data[y * canvas_width + x] = 0xFF;
        }
        for(int32_t x = right_cx; x < box_x1; x++) {
            if(!touchpad_test_v2_oval_contains(x, y)) data[y * canvas_width + x] = 0xFF;
        }
    }

    // capsule outline (drawn last so the mask above doesn't eat into it)
    render_draw_round_rectangle(
        model->canvas,
        TOUCHPAD_OVAL_MARGIN_X,
        TOUCHPAD_OVAL_MARGIN_Y,
        TOUCHPAD_OVAL_WIDTH,
        TOUCHPAD_OVAL_HEIGHT,
        TOUCHPAD_OVAL_RADIUS,
        1,
        color_black);

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

static bool touchpad_test_v2_app_input(InputEvent* event, void* context) {
    furi_check(context);
    TouchpadTestApp* instance = context;
    bool consumed = false;

    if(event->type == InputTypePress) {
        if(event->key == InputKeyBack) {
            furi_thread_signal(instance->thread, FuriSignalExit, NULL);
            consumed = true;
        } else if(event->key == InputKeyOk) {
            with_view_model(
                instance->view,
                TouchpadTestModel * model,
                {
                    TouchpadTestLineArray_reset(model->lines);
                    touchpad_test_v2_app_update_frame(model);
                },
                true);
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
                // start a fresh track on every new touch-down, discarding the previous one
                TouchpadTestLineArray_reset(model->lines);
                model->pressed = true;
                model->last_x = TOUCHPAD_OVAL_MARGIN_X + (event->x - touch_resolution_padding_x) * TOUCHPAD_OVAL_WIDTH / touch_resolution_x;
                model->last_y = TOUCHPAD_OVAL_MARGIN_Y + (event->y - touch_resolution_padding_y) * TOUCHPAD_OVAL_HEIGHT / touch_resolution_y;
#if TOUCHPAD_CLAMP_TO_OVAL
                touchpad_test_v2_clamp_to_oval(&model->last_x, &model->last_y);
#endif
                model->pressure = event->pressure / TOUCHPAD_RESOLUTION_PRESSURE;
                touchpad_test_v2_app_update_frame(model);
            },
            true);
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
                touchpad_test_v2_app_update_frame(model);
            },
            true);
        consumed = true;
        break;
    case InputTouchTypeEnd:
        with_view_model(
            instance->view,
            TouchpadTestModel * model,
            {
                model->pressed = false;
                model->pressure = event->pressure / TOUCHPAD_RESOLUTION_PRESSURE;
                touchpad_test_v2_app_update_frame(model);
            },
            true);
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
            model->canvas = canvas_alloc(TOUCHPAD_CANVAS_WIDTH, TOUCHPAD_CANVAS_HEIGHT);
            model->image = canvas_to_image(model->canvas);
            touchpad_test_v2_app_update_frame(model);
        },
        false);

    view_set_layout_callback(instance->view, touchpad_test_v2_app_layout);
    view_set_input_callback(instance->view, touchpad_test_v2_app_input, instance);
    view_set_input_touch_callback(instance->view, touchpad_test_v2_app_input_touch, instance);
    gui_add_view(instance->gui, instance->view, GuiViewPriorityApplication);
    return instance;
}

static void touchpad_test_v2_app_free(TouchpadTestApp* instance) {
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
    view_free(instance->view);
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
