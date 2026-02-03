#include <applications.h>
#include <gui/clay_helper.h>
#include <gui/gui.h>

#define DESKTOP_INPUT_QUEUE_SIZE       16
#define DESKTOP_INPUT_TOUCH_QUEUE_SIZE 16

#define TAG "DesktopSrv"

#define DESKTOP_MENU_ID(x) CLAY_SIDI(CLAY_STRING("Menu"), x)

typedef struct {
    Gui* gui;
    ViewPort* view_port;

    FuriEventLoop* event_loop;
    FuriMessageQueue* input_queue;
    FuriMessageQueue* input_touch_queue;

    uint32_t selected_index;
} Desktop;

typedef enum {
    DesktopEventTypeMax,
} DesktopEventType;

typedef struct {
    DesktopEventType type;
} DesktopEvent;

void desktop_layout(void* context) {
    Desktop* app = context;
    furi_check(app);

    Clay_Sizing layoutExpand = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};

    CLAY(
        CLAY_APP_ID("Container"),
        {
            .backgroundColor = COLOR_WHITE,
            .layout =
                {
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                    .childGap = 4,
                    .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                },
            .clip = {.vertical = true, .childOffset = Clay_GetScrollOffset()},
        }) {
        for(uint32_t i = 0; i < FLIPPER_APPS_COUNT; i++) {
            bool selected = (i == app->selected_index);
            CLAY(
                DESKTOP_MENU_ID(i),
                {
                    .layout =
                        {
                            .sizing = {.width = CLAY_SIZING_FIXED(120), .height = CLAY_SIZING_FIXED(13)},
                            .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                        },
                    .backgroundColor = selected ? COLOR_BLACK : COLOR_WHITE,
                    .cornerRadius = CLAY_CORNER_RADIUS(2),
                }) {
                Clay_String test_str = {
                    .isStaticallyAllocated = false,
                    .length = strlen(FLIPPER_APPS[i].name),
                    .chars = FLIPPER_APPS[i].name,
                };
                CLAY_TEXT(test_str, CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = selected ? COLOR_WHITE : COLOR_BLACK}));
            }
        }
    }
}

void desktop_post_layout(void* context) {
    Desktop* app = context;
    furi_check(app);

    Clay_ElementId scrollContainerId = CLAY_APP_ID("Container");
    Clay_ElementId targetChildId = DESKTOP_MENU_ID(app->selected_index);
    if(clay_helper_scroll_to_child(scrollContainerId, targetChildId, 0, 10, 15)) {
        gui_update(app->gui);
    }
}

static void desktop_input_logic(FuriEventLoopObject* object, void* context) {
    furi_check(context);
    Desktop* app = context;
    furi_check(object == app->input_queue);

    InputEvent event;
    furi_check(furi_message_queue_get(app->input_queue, &event, 0) == FuriStatusOk);

    if(event.key == InputKeyDown) {
        if(event.type == InputTypePress) {
            app->selected_index = (app->selected_index + 1) % FLIPPER_APPS_COUNT;
        }
    }
    if(event.key == InputKeyUp) {
        if(event.type == InputTypePress) {
            app->selected_index = (app->selected_index - 1 + FLIPPER_APPS_COUNT) % FLIPPER_APPS_COUNT;
        }
    }

    gui_update(app->gui);
}

static void desktop_touch_logic(FuriEventLoopObject* object, void* context) {
    furi_check(context);
    Desktop* app = context;
    furi_check(object == app->input_touch_queue);

    InputTouchEvent event;
    furi_check(furi_message_queue_get(app->input_touch_queue, &event, 0) == FuriStatusOk);
}

Desktop* desktop_alloc(void) {
    Desktop* app = malloc(sizeof(Desktop));
    app->gui = furi_record_open(RECORD_GUI);
    app->event_loop = furi_event_loop_alloc();
    app->input_queue = furi_message_queue_alloc(DESKTOP_INPUT_QUEUE_SIZE, sizeof(InputEvent));
    app->input_touch_queue = furi_message_queue_alloc(DESKTOP_INPUT_TOUCH_QUEUE_SIZE, sizeof(InputTouchEvent));

    app->view_port = view_port_alloc();
    view_port_set_layout_callback(app->view_port, desktop_layout, app);
    view_port_set_post_layout_callback(app->view_port, desktop_post_layout, app);
    view_port_set_input_callback(app->view_port, view_port_input_queue_glue, app->input_queue);
    view_port_set_input_touch_callback(app->view_port, view_port_input_touch_queue_glue, app->input_touch_queue);

    furi_event_loop_subscribe_message_queue(app->event_loop, app->input_queue, FuriEventLoopEventIn, desktop_input_logic, app);
    furi_event_loop_subscribe_message_queue(app->event_loop, app->input_touch_queue, FuriEventLoopEventIn, desktop_touch_logic, app);

    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    return app;
}

int32_t desktop_srv(void* p) {
    Desktop* app = desktop_alloc();
    furi_event_loop_run(app->event_loop);
    return 0;
}
