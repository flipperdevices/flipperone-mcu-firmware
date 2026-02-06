#include <applications.h>
#include <gui/clay_helper.h>
#include <gui/gui.h>

#define DESKTOP_INPUT_QUEUE_SIZE       16
#define DESKTOP_INPUT_TOUCH_QUEUE_SIZE 16
#define DESKTOP_APP_MESSAGE_QUEUE_SIZE 4

#define TAG "DesktopSrv"

#define DESKTOP_MENU_ID(x) CLAY_SIDI(CLAY_STRING("Menu"), x)

typedef enum {
    DesktopMessageTypeAppStart,
    DesktopMessageTypeAppClosed,
} DesktopMessageType;

typedef struct {
    DesktopMessageType type;
    FlipperInternalApplication* app;
    const char* args;
} DesktopMessage;

typedef struct {
    bool running;
    char* args;
    FuriThread* thread;
} DesktopApp;

typedef struct {
    Gui* gui;
    ViewPort* view_port;

    FuriEventLoop* event_loop;
    FuriMessageQueue* input_queue;
    FuriMessageQueue* input_touch_queue;

    uint32_t selected_index;

    DesktopApp app;
    FuriMessageQueue* app_message_queue;
} Desktop;

typedef enum {
    DesktopEventTypeMax,
} DesktopEventType;

typedef struct {
    DesktopEventType type;
} DesktopEvent;

static void desktop_layout(void* context) {
    Desktop* desktop = context;
    furi_check(desktop);

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
            bool selected = (i == desktop->selected_index);
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

static void desktop_post_layout(void* context) {
    Desktop* desktop = context;
    furi_check(desktop);

    Clay_ElementId scrollContainerId = CLAY_APP_ID("Container");
    Clay_ElementId targetChildId = DESKTOP_MENU_ID(desktop->selected_index);
    if(clay_helper_scroll_to_child(scrollContainerId, targetChildId, 0, 10, 15)) {
        view_port_update(desktop->view_port);
    }
}

static void desktop_app_thread_state_callback(FuriThread* thread, FuriThreadState thread_state, void* context) {
    UNUSED(thread);
    furi_assert(context);

    if(thread_state == FuriThreadStateStopped) {
        Desktop* desktop = context;

        DesktopMessage message;
        message.type = DesktopMessageTypeAppClosed;
        furi_message_queue_put(desktop->app_message_queue, &message, FuriWaitForever);
    }
}

static void desktop_start_app_thread(Desktop* desktop, FlipperInternalApplicationFlag flags) {
    // setup heap trace
    furi_thread_enable_heap_trace(desktop->app.thread);

    // setup thread state callbacks
    furi_thread_set_state_context(desktop->app.thread, desktop);
    furi_thread_set_state_callback(desktop->app.thread, desktop_app_thread_state_callback);

    // start app thread
    furi_thread_start(desktop->app.thread);
}

static void desktop_start_internal_app(Desktop* desktop, const FlipperInternalApplication* app, const char* args) {
    FURI_LOG_I(TAG, "Starting %s", app->name);

    // store args
    furi_assert(desktop->app.args == NULL);
    if(args && strlen(args) > 0) {
        desktop->app.args = strdup(args);
    }

    desktop->app.thread = furi_thread_alloc_ex(app->name, app->stack_size, app->app, desktop->app.args);
    furi_thread_set_appid(desktop->app.thread, app->appid);

    desktop_start_app_thread(desktop, app->flags);
}

static void desktop_input_logic(FuriEventLoopObject* object, void* context) {
    furi_check(context);
    Desktop* desktop = context;
    furi_check(object == desktop->input_queue);

    InputEvent event;
    furi_check(furi_message_queue_get(desktop->input_queue, &event, 0) == FuriStatusOk);
    if(event.type == InputTypePress) {
        switch(event.key) {
        case InputKeyOk: {
            DesktopMessage message;
            message.type = DesktopMessageTypeAppStart;
            message.app = (FlipperInternalApplication*)&FLIPPER_APPS[desktop->selected_index];
            message.args = NULL;
            furi_message_queue_put(desktop->app_message_queue, &message, FuriWaitForever);
            break;
        }
        case InputKeyDown:
            desktop->selected_index = (desktop->selected_index + 1) % FLIPPER_APPS_COUNT;
            break;
        case InputKeyUp:
            desktop->selected_index = (desktop->selected_index - 1 + FLIPPER_APPS_COUNT) % FLIPPER_APPS_COUNT;
            break;
        default:
            break;
        }
    }
    view_port_update(desktop->view_port);
}

static void desktop_touch_logic(FuriEventLoopObject* object, void* context) {
    furi_check(context);
    Desktop* desktop = context;
    furi_check(object == desktop->input_touch_queue);

    InputTouchEvent event;
    furi_check(furi_message_queue_get(desktop->input_touch_queue, &event, 0) == FuriStatusOk);
}

static void desktop_do_app_closed(Desktop* desktop) {
    furi_assert(desktop->app.thread);

    furi_thread_join(desktop->app.thread);
    FURI_LOG_I(TAG, "App returned: %li", furi_thread_get_return_code(desktop->app.thread));

    if(desktop->app.args) {
        free(desktop->app.args);
        desktop->app.args = NULL;
    }

    furi_thread_free(desktop->app.thread);
    desktop->app.thread = NULL;

    FURI_LOG_I(TAG, "Application stopped. Free heap: %zu", memmgr_get_free_heap());
}

static void desktop_app_message_logic(FuriEventLoopObject* object, void* context) {
    furi_check(context);
    Desktop* desktop = context;
    furi_check(object == desktop->app_message_queue);

    DesktopMessage message;
    furi_check(furi_message_queue_get(desktop->app_message_queue, &message, 0) == FuriStatusOk);

    switch(message.type) {
    case DesktopMessageTypeAppStart:
        if(desktop->app.running) {
            FURI_LOG_E(TAG, "App start requested, but another app is already running");
        } else {
            desktop->app.running = true;
            desktop_start_internal_app(desktop, message.app, message.args);
        }
        break;
    case DesktopMessageTypeAppClosed:
        furi_check(desktop->app.running);
        desktop_do_app_closed(desktop);
        desktop->app.running = false;
        break;
    default:
        furi_assert(false);
        break;
    }
}

static Desktop* desktop_alloc(void) {
    Desktop* desktop = malloc(sizeof(Desktop));
    desktop->gui = furi_record_open(RECORD_GUI);
    desktop->event_loop = furi_event_loop_alloc();
    desktop->input_queue = furi_message_queue_alloc(DESKTOP_INPUT_QUEUE_SIZE, sizeof(InputEvent));
    desktop->input_touch_queue = furi_message_queue_alloc(DESKTOP_INPUT_TOUCH_QUEUE_SIZE, sizeof(InputTouchEvent));
    desktop->app_message_queue = furi_message_queue_alloc(DESKTOP_APP_MESSAGE_QUEUE_SIZE, sizeof(DesktopMessage));

    desktop->view_port = view_port_alloc();
    view_port_set_layout_callback(desktop->view_port, desktop_layout, desktop);
    view_port_set_post_layout_callback(desktop->view_port, desktop_post_layout, desktop);
    view_port_set_input_callback(desktop->view_port, view_port_input_queue_glue, desktop->input_queue);
    view_port_set_input_touch_callback(desktop->view_port, view_port_input_touch_queue_glue, desktop->input_touch_queue);

    furi_event_loop_subscribe_message_queue(desktop->event_loop, desktop->input_queue, FuriEventLoopEventIn, desktop_input_logic, desktop);
    furi_event_loop_subscribe_message_queue(desktop->event_loop, desktop->input_touch_queue, FuriEventLoopEventIn, desktop_touch_logic, desktop);
    furi_event_loop_subscribe_message_queue(desktop->event_loop, desktop->app_message_queue, FuriEventLoopEventIn, desktop_app_message_logic, desktop);

    gui_add_view_port(desktop->gui, desktop->view_port, GuiLayerFullscreen);

    return desktop;
}

int32_t desktop_srv(void* p) {
    Desktop* desktop = desktop_alloc();
    furi_event_loop_run(desktop->event_loop);
    return 0;
}
