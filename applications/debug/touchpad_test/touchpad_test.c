#include <furi.h>
#include <gui/gui.h>
#include <gui/clay_helper.h>

#define TAG "TouchpadTest"

typedef struct {
    uint32_t x;
    uint32_t y;
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

    float touch_x = (model->x - 180.f) / 2.8f;
    float touch_y = (model->y - 180.f) / 5.f;

    CLAY(
        CLAY_APP_ID("OuterContainer"),
        {.backgroundColor = COLOR_WHITE,
         .layout = {
             .layoutDirection = CLAY_TOP_TO_BOTTOM,
             .sizing = layoutExpand,
             .padding = {4, 4, 4, 3},
             .childGap = 4,
         }}) {
        CLAY(
            CLAY_APP_ID("Header"),
            {
                .layout =
                    {
                        .sizing = {.height = CLAY_SIZING_FIXED(14), .width = CLAY_SIZING_GROW(0)},
                        .childGap = 8,
                        .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                    },
            }) {
            CLAY_AUTO_ID({.layout = {.padding = {8, 8, 4, 4}}}) {
                CLAY_TEXT(CLAY_STRING("Touchpad Test"), CLAY_TEXT_CONFIG({.fontId = FontButton, .textColor = COLOR_BLACK}));
            }
        }
        CLAY(
            CLAY_APP_ID("MainContent"),
            {
                .border = contentBorders,
                .cornerRadius = CLAY_CORNER_RADIUS(60),
                .clip = {.vertical = true},
                .layout =
                    {
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        .childGap = 8,
                        .padding = {6, 6, 6, 6},
                        .sizing = layoutExpand,
                        .childAlignment = {.y = CLAY_ALIGN_Y_TOP, .x = CLAY_ALIGN_X_LEFT},
                    },
            }) {
            CLAY_AUTO_ID({
                .border = {.color = COLOR_BLACK, .width = {1, 1, 1, 1}},
                .floating =
                    {
                        .offset = {.x = touch_x, .y = touch_y},
                        .attachTo = CLAY_ATTACH_TO_PARENT,
                    },
                .layout =
                    {
                        .padding = {8, 8, 4, 4},
                        .sizing = {.width = CLAY_SIZING_FIXED(0), .height = CLAY_SIZING_FIXED(15)},
                    },
                .backgroundColor = model->pressed ? COLOR_BLACK : COLOR_WHITE,
                .cornerRadius = CLAY_CORNER_RADIUS(8),
            }) {
            }
        }
    }

    return false;
}

static bool touchpad_test_app_input(InputEvent* event, void* context) {
    furi_check(context);
    TouchpadTestApp* instance = context;
    bool consumed = false;

    if(event->key == InputKeyBack) {
        if(event->type == InputTypePress) {
            furi_thread_signal(instance->thread, FuriSignalExit, NULL);
            consumed = true;
        }
    }

    return consumed;
}

static bool touchpad_test_app_input_touch(InputTouchEvent* event, void* context) {
    furi_check(context);
    TouchpadTestApp* instance = context;
    bool consumed = false;

    switch(event->type) {
    case InputTouchTypeStart:
        with_view_model(
            instance->view,
            TouchpadTestModel * model,
            {
                model->pressed = true;
                model->x = event->x;
                model->y = event->y;
            },
            true);
        consumed = true;
        break;
    case InputTouchTypeMove:
        with_view_model(
            instance->view,
            TouchpadTestModel * model,
            {
                model->x = event->x;
                model->y = event->y;
            },
            true);
        consumed = true;
        break;
    case InputTouchTypeEnd:
        with_view_model(instance->view, TouchpadTestModel * model, { model->pressed = false; }, true);
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
    view_set_layout_callback(instance->view, touchpad_test_app_layout);
    view_set_input_callback(instance->view, touchpad_test_app_input, instance);
    view_set_input_touch_callback(instance->view, touchpad_test_app_input_touch, instance);
    gui_add_view(instance->gui, instance->view, GuiViewPriorityApplication);
    return instance;
}

static void touchpad_test_app_free(TouchpadTestApp* instance) {
    gui_remove_view(instance->gui, instance->view);
    furi_record_close(RECORD_GUI);
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
