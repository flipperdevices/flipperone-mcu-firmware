#include <furi.h>
#include <gui/gui.h>
#include <gui/clay_helper.h>

#define TAG "TouchpadTest"

#define APP_INPUT_QUEUE_SIZE       16
#define APP_INPUT_TOUCH_QUEUE_SIZE 16

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriEventLoop* event_loop;
    FuriMessageQueue* input_queue;
    FuriMessageQueue* input_touch_queue;

    uint32_t x;
    uint32_t y;
    bool pressed;
} App;

static void app_layout(void* context) {
    furi_assert(context);
    App* app = (App*)context;

    Clay_Sizing layoutExpand = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
    Clay_BorderElementConfig contentBorders = {.color = COLOR_BLACK, .width = {.top = 1, .left = 1, .right = 1, .bottom = 1}};

    float touch_x = (app->x - 180.f) / 2.8f;
    float touch_y = (app->y - 180.f) / 5.f;

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
                .backgroundColor = app->pressed ? COLOR_BLACK : COLOR_WHITE,
                .cornerRadius = CLAY_CORNER_RADIUS(8),
            }) {
            }
        }
    }
}

static void app_input_logic(FuriEventLoopObject* object, void* context) {
    furi_check(context);
    App* app = context;
    furi_check(object == app->input_queue);

    InputEvent event;
    furi_check(furi_message_queue_get(app->input_queue, &event, 0) == FuriStatusOk);

    if(event.key == InputKeyBack) {
        if(event.type == InputTypePress) {
            furi_thread_signal(furi_thread_get_current(), FuriSignalExit, NULL);
        }
    }
}

static void app_input_touch_logic(FuriEventLoopObject* object, void* context) {
    furi_check(context);
    App* app = context;
    furi_check(object == app->input_touch_queue);

    InputTouchEvent event;
    furi_check(furi_message_queue_get(app->input_touch_queue, &event, 0) == FuriStatusOk);

    switch(event.type) {
    case InputTouchTypeStart:
        app->pressed = true;
        app->x = event.x;
        app->y = event.y;
        gui_update(app->gui);
        break;
    case InputTouchTypeMove:
        app->x = event.x;
        app->y = event.y;
        gui_update(app->gui);
        break;
    case InputTouchTypeEnd:
        app->pressed = false;
        gui_update(app->gui);
        break;
    }
}

static App* app_alloc(void) {
    App* app = malloc(sizeof(App));
    app->gui = furi_record_open(RECORD_GUI);
    app->event_loop = furi_event_loop_alloc();
    app->input_queue = furi_message_queue_alloc(APP_INPUT_QUEUE_SIZE, sizeof(InputEvent));
    app->input_touch_queue = furi_message_queue_alloc(APP_INPUT_TOUCH_QUEUE_SIZE, sizeof(InputTouchEvent));

    app->view_port = view_port_alloc();
    view_port_set_layout_callback(app->view_port, app_layout, app);
    view_port_set_input_callback(app->view_port, view_port_input_queue_glue, app->input_queue);
    view_port_set_input_touch_callback(app->view_port, view_port_input_touch_queue_glue, app->input_touch_queue);
    furi_event_loop_subscribe_message_queue(app->event_loop, app->input_queue, FuriEventLoopEventIn, app_input_logic, app);
    furi_event_loop_subscribe_message_queue(app->event_loop, app->input_touch_queue, FuriEventLoopEventIn, app_input_touch_logic, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    return app;
}

static void app_free(App* app) {
    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close(RECORD_GUI);

    furi_event_loop_unsubscribe(app->event_loop, app->input_queue);
    furi_event_loop_unsubscribe(app->event_loop, app->input_touch_queue);
    view_port_free(app->view_port);
    furi_message_queue_free(app->input_queue);
    furi_message_queue_free(app->input_touch_queue);
    furi_event_loop_free(app->event_loop);
    free(app);
}

int32_t touchpad_test_app(void* p) {
    App* app = app_alloc();
    furi_event_loop_run(app->event_loop);
    app_free(app);
    return 0;
}
