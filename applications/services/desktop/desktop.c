#include "desktop.h"
#include "desktop_i.h"
#include <applications.h>
#include <gui/clay_helper.h>
#include <gui/gui.h>
#include <assets.h>
#include "scenes/scenes.h"

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

struct Desktop {
    Gui* gui;

    Scene* main_scene;
    Scene* header_scene;
    Scene* power_menu_scene;

    FuriEventLoop* event_loop;
    DesktopApp app;
    FuriMessageQueue* app_message_queue;
};

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

    furi_event_loop_subscribe_message_queue(desktop->event_loop, desktop->app_message_queue, FuriEventLoopEventIn, desktop_app_message_logic, desktop);

    desktop->header_scene = scene_alloc(&scene_header_callbacks, desktop);
    desktop->main_scene = scene_alloc(&scene_desktop_callbacks, desktop);
    desktop->power_menu_scene = scene_alloc(&scene_power_menu_callbacks, desktop);

    gui_add_view(desktop->gui, scene_get_view(desktop->header_scene), GuiViewPriorityHeader);
    gui_add_view(desktop->gui, scene_get_view(desktop->main_scene), GuiViewPriorityDesktop);
    gui_add_view(desktop->gui, scene_get_view(desktop->power_menu_scene), GuiViewPriorityMenu);

    scene_enter(desktop->header_scene, desktop);
    scene_enter(desktop->main_scene, desktop);

    furi_record_create(RECORD_DESKTOP, desktop);

    return desktop;
}

void desktop_show_power_menu(Desktop* desktop) {
    furi_check(desktop);
    scene_enter(desktop->power_menu_scene, desktop);
}

void desktop_hide_power_menu(Desktop* desktop) {
    furi_check(desktop);
    scene_exit(desktop->power_menu_scene, desktop);
}

bool desktop_get_power_menu_state(Desktop* desktop) {
    furi_check(desktop);
    return view_is_enabled(scene_get_view(desktop->power_menu_scene));
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
