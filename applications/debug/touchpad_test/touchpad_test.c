#include <furi.h>
#include <gui/gui.h>
#include <gui/clay_helper.h>
#include <m-array.h>
#include <m-algo.h>

#define TAG "TouchpadTest"

#define TOUCHPAD_CANVAS_WIDTH  (256)
#define TOUCHPAD_HEADER_HEIGHT (22)
#define TOUCHPAD_CANVAS_HEIGHT (144 - TOUCHPAD_HEADER_HEIGHT)
#define TOUCHPAD_ELLIPSE_COUNT (5)

typedef struct TouchpadTestLine {
    int32_t x0;
    int32_t y0;
    int32_t x1;
    int32_t y1;
} TouchpadTestLine;

#define TOUCHPAD_MAX_LINES_COUNT (32 * 1024 / sizeof(TouchpadTestLine))

ARRAY_DEF(TouchpadTestLineArray, TouchpadTestLine, M_POD_OPLIST);
#define M_OPL_TouchpadTestLineArray_t() ARRAY_OPLIST(TouchpadTestLineArray, M_POD_OPLIST)
ALGO_DEF(TouchpadTestLineArray, TouchpadTestLineArray_t);

typedef struct {
    Canvas* canvas;
    Image image;
    TouchpadTestLineArray_t lines;

    int32_t last_x;
    int32_t last_y;
    bool pressed;

} TouchpadTestModel;

typedef struct {
    Gui* gui;
    View* view;
    FuriEventLoop* event_loop;
    FuriThread* thread;
} TouchpadTestApp;

static bool touchpad_test_app_layout(void* _model) {
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
            CLAY_AUTO_ID({.layout = {.padding = {8, 8, 4, 4}}}) {
                CLAY_TEXT(CLAY_STRING("Touchpad Test"), CLAY_TEXT_CONFIG({.fontId = FontButton, .textColor = COLOR_BLACK}));
            }
            CLAY_AUTO_ID({
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

void touchpad_test_app_update_frame(TouchpadTestModel* model) {
    canvas_clear(model->canvas, 0xFF);

    // ellipses
    for(int i = 0; i < TOUCHPAD_ELLIPSE_COUNT; i++) {
        int32_t margin_x = i * (TOUCHPAD_CANVAS_WIDTH / 2) / TOUCHPAD_ELLIPSE_COUNT;
        int32_t margin_y = i * (TOUCHPAD_CANVAS_HEIGHT / 2) / TOUCHPAD_ELLIPSE_COUNT;
        int32_t w = TOUCHPAD_CANVAS_WIDTH - 2 * margin_x;
        int32_t h = TOUCHPAD_CANVAS_HEIGHT - 2 * margin_y;
        int32_t r = 60 - i * (60 / TOUCHPAD_ELLIPSE_COUNT);
        uint8_t color = (i == 0) ? 0x00 : 220;
        render_draw_round_rectangle(model->canvas, margin_x, margin_y, w, h, r, 1, color);
    }

    // grid
    render_draw_line(model->canvas, TOUCHPAD_CANVAS_WIDTH / 2, 0, TOUCHPAD_CANVAS_WIDTH / 2, TOUCHPAD_CANVAS_HEIGHT, 220);
    render_draw_line(model->canvas, 60, 0, 60, TOUCHPAD_CANVAS_HEIGHT, 220);
    render_draw_line(model->canvas, TOUCHPAD_CANVAS_WIDTH - 60, 0, TOUCHPAD_CANVAS_WIDTH - 60, TOUCHPAD_CANVAS_HEIGHT, 220);
    render_draw_line(model->canvas, 0, TOUCHPAD_CANVAS_HEIGHT / 2, TOUCHPAD_CANVAS_WIDTH, TOUCHPAD_CANVAS_HEIGHT / 2, 220);

    // touch lines
    for(size_t i = 0; i < TouchpadTestLineArray_size(model->lines); i++) {
        TouchpadTestLine* line = TouchpadTestLineArray_get(model->lines, i);
        render_draw_line(model->canvas, line->x0, line->y0, line->x1, line->y1, 0x00);
    }

    // touch point
    const int32_t radius = 3;
    if(model->pressed) {
        render_fill_round_rectangle(model->canvas, model->last_x - radius, model->last_y - radius, 2 * radius, 2 * radius, radius, 0x00);
    } else {
        render_draw_round_rectangle(model->canvas, model->last_x - radius, model->last_y - radius, 2 * radius, 2 * radius, radius, 1, 0x00);
    }
}

static bool touchpad_test_app_input(InputEvent* event, void* context) {
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
                    touchpad_test_app_update_frame(model);
                },
                true);
            consumed = true;
        }
    }

    return consumed;
}

static void touchpad_test_app_model_push_line(TouchpadTestModel* model, int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    TouchpadTestLine line = {.x0 = x0, .y0 = y0, .x1 = x1, .y1 = y1};
    TouchpadTestLineArray_push_back(model->lines, line);

    while(TouchpadTestLineArray_size(model->lines) > TOUCHPAD_MAX_LINES_COUNT) {
        TouchpadTestLineArray_pop_at(NULL, model->lines, 0);
    }
}

static bool touchpad_test_app_input_touch(InputTouchEvent* event, void* context) {
    furi_check(context);
    TouchpadTestApp* instance = context;
    bool consumed = false;
    float scale_x = 0.7f;
    float scale_y = 0.6f;
    const int32_t touch_real_resolution_x = 1024;
    const int32_t touch_real_resolution_y = 768;
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
                model->pressed = true;
                model->last_x = (event->x - touch_resolution_padding_x) * TOUCHPAD_CANVAS_WIDTH / touch_resolution_x;
                model->last_y = (event->y - touch_resolution_padding_y) * TOUCHPAD_CANVAS_HEIGHT / touch_resolution_y;
                touchpad_test_app_update_frame(model);
            },
            true);
        consumed = true;
        break;
    case InputTouchTypeMove:
        with_view_model(
            instance->view,
            TouchpadTestModel * model,
            {
                int32_t new_x = (event->x - touch_resolution_padding_x) * TOUCHPAD_CANVAS_WIDTH / touch_resolution_x;
                int32_t new_y = (event->y - touch_resolution_padding_y) * TOUCHPAD_CANVAS_HEIGHT / touch_resolution_y;
                if(model->pressed) {
                    touchpad_test_app_model_push_line(model, model->last_x, model->last_y, new_x, new_y);
                }
                model->last_x = new_x;
                model->last_y = new_y;
                touchpad_test_app_update_frame(model);
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
                touchpad_test_app_update_frame(model);
            },
            true);
        consumed = true;
        break;
    default:
        break;
    }

    return consumed;
}

static TouchpadTestApp* touchpad_test_app_alloc(void) {
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
            touchpad_test_app_update_frame(model);
        },
        false);

    view_set_layout_callback(instance->view, touchpad_test_app_layout);
    view_set_input_callback(instance->view, touchpad_test_app_input, instance);
    view_set_input_touch_callback(instance->view, touchpad_test_app_input_touch, instance);
    gui_add_view(instance->gui, instance->view, GuiViewPriorityApplication);
    return instance;
}

static void touchpad_test_app_free(TouchpadTestApp* instance) {
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

int32_t touchpad_test_app(void* p) {
    UNUSED(p);
    TouchpadTestApp* instance = touchpad_test_app_alloc();
    furi_event_loop_run(instance->event_loop);
    touchpad_test_app_free(instance);
    return 0;
}
