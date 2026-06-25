#include "desktop.h"
#include <applications.h>
#include <gui/clay_helper.h>
#include <gui/gui.h>
#include <assets.h>

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
    View* header_view;

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

static void desktop_softkey_button_element(const char* text, bool active) {
    const Image* image = active ? &button_pressed : &button_released;
    const Clay_Sizing sizing = {.width = CLAY_SIZING_FIXED(image->width), .height = CLAY_SIZING_FIXED(image->height)};

    CLAY_AUTO_ID({
        .layout =
            {
                .sizing = sizing,
            },
    }) {
        CLAY_AUTO_ID({
            .layout =
                {
                    .sizing = sizing,
                    .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                    .padding = {.left = 0, .right = 0, .top = 3, .bottom = 0},
                },
            .image = {.imageData = (void*)image},
        }) {
            CLAY_TEXT(clay_helper_string_from_chars(text), CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = COLOR_BLACK}));
        }
    }
}

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
                    .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                },
            .clip = {.vertical = true, .childOffset = Clay_GetScrollOffset()},
        }) {
        CLAY_AUTO_ID({
            .layout =
                {
                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                    .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                },
        }) {
            clay_fixed_image(&face_sleep);
        }

        CLAY_AUTO_ID({
            .layout =
                {
                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)},
                    .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                    .childGap = 4,
                },
        }) {
            desktop_softkey_button_element("Help", false);
            desktop_softkey_button_element("Power", false);
            desktop_softkey_button_element("Settings", false);
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
                .args = FLIPPER_APPS[selected_index].args,
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

#include <version.h>

typedef struct {
    FuriString* version_text;
    FuriString* charge_text;
} DesktopHeaderModel;

static const Clay_Color COLOR_VERSION = {0x69, 0x69, 0x69, 255};

static bool desktop_header_layout(void* _model) {
    furi_assert(_model);
    DesktopHeaderModel* model = _model;

    CLAY(
        CLAY_APP_ID("Container"),
        {
            .backgroundColor = COLOR_WHITE,
            .layout =
                {
                    .layoutDirection = CLAY_LEFT_TO_RIGHT,
                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(11)},
                    .childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER},
                    .padding = {.left = 3, .right = 2, .top = 0, .bottom = 0},
                },
            .floating =
                {
                    .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_TOP, .parent = CLAY_ATTACH_POINT_CENTER_TOP},
                    .attachTo = CLAY_ATTACH_TO_ROOT,
                },
        }) {
        CLAY_AUTO_ID({
            .layout =
                {
                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                    .childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER},
                    .padding = {.left = 0, .right = 0, .top = 2, .bottom = 0},

                },
        }) {
            CLAY_TEXT(
                clay_helper_string_from(model->version_text),
                CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = COLOR_VERSION, .wrapMode = CLAY_TEXT_WRAP_NONE}));
        }
        CLAY_AUTO_ID({
            .layout =
                {
                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                    .childAlignment = {.x = CLAY_ALIGN_X_RIGHT, .y = CLAY_ALIGN_Y_CENTER},
                    .padding = {.left = 0, .right = 0, .top = 2, .bottom = 0},
                },
        }) {
            CLAY_TEXT(clay_helper_string_from(model->charge_text), CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = COLOR_BLACK}));
        }
        CLAY_AUTO_ID() {
            clay_fixed_image(&battery);
        }
    }
    return false;
}

static View* desktop_alloc_header_view(Desktop* desktop) {
    View* view = view_alloc();
    view_allocate_model(view, ViewModelTypeLockFree, sizeof(DesktopHeaderModel));
    view_set_layout_callback(view, desktop_header_layout);
    view_set_transparent(view, true);

    with_view_model(
        view,
        DesktopHeaderModel * model,
        {
            const Version* version = version_get();

            model->version_text = furi_string_alloc();
            furi_string_printf(model->version_text, "%s %s", version_get_gitbranch(version), version_get_githash(version));

            // TODO: charge bar
            model->charge_text = furi_string_alloc();
            furi_string_printf(model->charge_text, "-1%%");
        },
        false);

    return view;
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

    desktop->header_view = desktop_alloc_header_view(desktop);

    gui_add_view(desktop->gui, desktop->view, GuiViewPriorityDesktop);
    gui_add_view(desktop->gui, desktop->header_view, GuiViewPriorityHeader);

    furi_record_create(RECORD_DESKTOP, desktop);

    return desktop;
}

int32_t desktop_srv(void* p) {
    UNUSED(p);
    Desktop* desktop = desktop_alloc();
    furi_event_loop_run(desktop->event_loop);
    return 0;
}

bool desktop_start_app(const FlipperInternalApplication* app) {
    furi_assert(app);

    Desktop* desktop = furi_record_open(RECORD_DESKTOP);
    DesktopMessage message = {
        .type = DesktopMessageTypeAppStart,
        .app = app,
        .args = app->args,
    };

    furi_message_queue_put(desktop->app_message_queue, &message, FuriWaitForever);
    furi_record_close(RECORD_DESKTOP);

    return true;
}
