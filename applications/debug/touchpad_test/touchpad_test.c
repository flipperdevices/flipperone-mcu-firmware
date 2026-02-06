#include <furi.h>
#include <gui/gui.h>
#include <gui/clay_helper.h>

#define TAG "TouchpadTest"

#define TOUCHPAD_TEST_INPUT_QUEUE_SIZE       16
#define TOUCHPAD_TEST_INPUT_TOUCH_QUEUE_SIZE 16

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriEventLoop* event_loop;
    FuriMessageQueue* input_queue;
    FuriMessageQueue* input_touch_queue;

    uint32_t x;
    uint32_t y;
    bool pressed;
} TouchpadTestApp;

static void touchpad_test_app_layout(void* context) {
    furi_assert(context);
    TouchpadTestApp* instance = (TouchpadTestApp*)context;

    Clay_Sizing layoutExpand = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
    Clay_BorderElementConfig contentBorders = {.color = COLOR_BLACK, .width = {.top = 1, .left = 1, .right = 1, .bottom = 1}};

    float touch_x = (instance->x - 180.f) / 2.8f;
    float touch_y = (instance->y - 180.f) / 5.f;

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
                .backgroundColor = instance->pressed ? COLOR_BLACK : COLOR_WHITE,
                .cornerRadius = CLAY_CORNER_RADIUS(8),
            }) {
            }
        }
    }
}

static void touchpad_test_app_input_logic(FuriEventLoopObject* object, void* context) {
    furi_check(context);
    TouchpadTestApp* instance = context;
    furi_check(object == instance->input_queue);

    InputEvent event;
    furi_check(furi_message_queue_get(instance->input_queue, &event, 0) == FuriStatusOk);

    if(event.key == InputKeyBack) {
        if(event.type == InputTypePress) {
            furi_thread_signal(furi_thread_get_current(), FuriSignalExit, NULL);
        }
    }
}

static void touchpad_test_app_input_touch_logic(FuriEventLoopObject* object, void* context) {
    furi_check(context);
    TouchpadTestApp* instance = context;
    furi_check(object == instance->input_touch_queue);

    InputTouchEvent event;
    furi_check(furi_message_queue_get(instance->input_touch_queue, &event, 0) == FuriStatusOk);

    switch(event.type) {
    case InputTouchTypeStart:
        instance->pressed = true;
        instance->x = event.x;
        instance->y = event.y;
        view_port_update(instance->view_port);
        break;
    case InputTouchTypeMove:
        instance->x = event.x;
        instance->y = event.y;
        view_port_update(instance->view_port);
        break;
    case InputTouchTypeEnd:
        instance->pressed = false;
        view_port_update(instance->view_port);
        break;
    }
}

static TouchpadTestApp* touchpad_test_app_alloc(void) {
    TouchpadTestApp* instance = malloc(sizeof(TouchpadTestApp));
    instance->gui = furi_record_open(RECORD_GUI);
    instance->event_loop = furi_event_loop_alloc();
    instance->input_queue = furi_message_queue_alloc(TOUCHPAD_TEST_INPUT_QUEUE_SIZE, sizeof(InputEvent));
    instance->input_touch_queue = furi_message_queue_alloc(TOUCHPAD_TEST_INPUT_TOUCH_QUEUE_SIZE, sizeof(InputTouchEvent));

    instance->view_port = view_port_alloc();
    view_port_set_layout_callback(instance->view_port, touchpad_test_app_layout, instance);
    view_port_set_input_callback(instance->view_port, view_port_input_queue_glue, instance->input_queue);
    view_port_set_input_touch_callback(instance->view_port, view_port_input_touch_queue_glue, instance->input_touch_queue);
    furi_event_loop_subscribe_message_queue(instance->event_loop, instance->input_queue, FuriEventLoopEventIn, touchpad_test_app_input_logic, instance);
    furi_event_loop_subscribe_message_queue(
        instance->event_loop, instance->input_touch_queue, FuriEventLoopEventIn, touchpad_test_app_input_touch_logic, instance);
    gui_add_view_port(instance->gui, instance->view_port, GuiLayerFullscreen);
    return instance;
}

static void touchpad_test_app_free(TouchpadTestApp* instance) {
    gui_remove_view_port(instance->gui, instance->view_port);
    furi_record_close(RECORD_GUI);

    furi_event_loop_unsubscribe(instance->event_loop, instance->input_queue);
    furi_event_loop_unsubscribe(instance->event_loop, instance->input_touch_queue);
    view_port_free(instance->view_port);
    furi_message_queue_free(instance->input_queue);
    furi_message_queue_free(instance->input_touch_queue);
    furi_event_loop_free(instance->event_loop);
    free(instance);
}

int32_t touchpad_test_app(void* p) {
    TouchpadTestApp* instance = touchpad_test_app_alloc();
    furi_event_loop_run(instance->event_loop);
    touchpad_test_app_free(instance);
    return 0;
}
