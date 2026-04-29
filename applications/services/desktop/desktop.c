#include <applications.h>
#include <gui/clay_helper.h>
#include <gui/gui.h>

#define DESKTOP_INPUT_QUEUE_SIZE       16
#define DESKTOP_INPUT_TOUCH_QUEUE_SIZE 16
#define DESKTOP_APP_MESSAGE_QUEUE_SIZE 4

#define TAG "DesktopSrv"

#define DESKTOP_MENU_ID(x) CLAY_SIDI(CLAY_STRING("DesktopMenu"), x)

typedef enum {
    DesktopMessageTypeAppStart,
    DesktopMessageTypeAppClosed,
} DesktopMessageType;

typedef struct {
    DesktopMessageType type;
    const FlipperInternalApplication* app;
    const char* args;
} DesktopMessage;

typedef struct {
    bool running;
    char* args;
    FuriThread* thread;
} DesktopApp;

typedef struct {
    uint32_t selected_index;
} DesktopModel;

typedef struct {
    Gui* gui;
    View* view;

    FuriEventLoop* event_loop;
    DesktopApp app;
    FuriMessageQueue* app_message_queue;
} Desktop;

typedef enum {
    DesktopEventTypeMax,
} DesktopEventType;

typedef struct {
    DesktopEventType type;
} DesktopEvent;

static bool desktop_layout(void* _model) {
    DesktopModel* model = _model;
    furi_check(model);

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
            bool selected = (i == model->selected_index);
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
                CLAY_TEXT(
                    clay_helper_string_from_chars(FLIPPER_APPS[i].name),
                    CLAY_TEXT_CONFIG({
                        .fontId = FontBody,
                        .textColor = selected ? COLOR_WHITE : COLOR_BLACK,
                    }));
            }
        }
    }

    return false;
}

static bool desktop_post_layout(void* _model) {
    DesktopModel* model = _model;
    furi_check(model);

    Clay_ElementId scrollContainerId = CLAY_APP_ID("Container");
    Clay_ElementId targetChildId = DESKTOP_MENU_ID(model->selected_index);

    if(clay_helper_scroll_to_child(scrollContainerId, targetChildId, 0, 10, 15)) {
        return true;
    }

    return false;
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

static void desktop_start_app_thread(Desktop* desktop) {
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

    desktop_start_app_thread(desktop);
}

static bool desktop_input(InputEvent* event, void* context) {
    furi_check(context);
    Desktop* desktop = context;
    bool consumed = false;

    if(event->type == InputTypePress) {
        switch(event->key) {
        case InputKeyOk: {
            uint32_t selected_index;
            with_view_model(desktop->view, DesktopModel * model, { selected_index = model->selected_index; }, false);

            DesktopMessage message = {
                .type = DesktopMessageTypeAppStart,
                .app = &FLIPPER_APPS[selected_index],
                .args = NULL,
            };

            furi_message_queue_put(desktop->app_message_queue, &message, FuriWaitForever);
            consumed = true;
            break;
        }
        case InputKeyDown:
            with_view_model(desktop->view, DesktopModel * model, { model->selected_index = (model->selected_index + 1) % FLIPPER_APPS_COUNT; }, true);
            consumed = true;
            break;
        case InputKeyUp:
            with_view_model(
                desktop->view, DesktopModel * model, { model->selected_index = (model->selected_index - 1 + FLIPPER_APPS_COUNT) % FLIPPER_APPS_COUNT; }, true);
            consumed = true;
            break;
        default:
            break;
        }
    }

    return consumed;
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
    desktop->app_message_queue = furi_message_queue_alloc(DESKTOP_APP_MESSAGE_QUEUE_SIZE, sizeof(DesktopMessage));

    desktop->view = view_alloc();
    view_allocate_model(desktop->view, ViewModelTypeLockFree, sizeof(DesktopModel));
    view_set_layout_callback(desktop->view, desktop_layout);
    view_set_post_layout_callback(desktop->view, desktop_post_layout);
    view_set_input_callback(desktop->view, desktop_input, desktop);
    furi_event_loop_subscribe_message_queue(desktop->event_loop, desktop->app_message_queue, FuriEventLoopEventIn, desktop_app_message_logic, desktop);

    gui_add_view(desktop->gui, desktop->view, GuiViewPriorityDesktop);

    return desktop;
}

int32_t desktop_srv(void* p) {
    UNUSED(p);
    Desktop* desktop = desktop_alloc();
    furi_event_loop_run(desktop->event_loop);
    return 0;
}
