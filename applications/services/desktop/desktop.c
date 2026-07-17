#include "desktop.h"
#include "desktop_i.h"
#include <applications.h>
#include <power/power.h>
#include <gui/clay_helper.h>
#include <gui/gui.h>
#include <assets.h>
#include "scenes/scenes.h"

#define DESKTOP_APP_MESSAGE_QUEUE_SIZE         4
#define DESKTOP_SCENE_EVENT_MESSAGE_QUEUE_SIZE 16

#define DESKTOP_POWER_UPDATE_PERIOD_MS 5000

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
} DesktopAppMessage;

typedef struct {
    uint32_t event;
    void* data;
} DesktopSceneEventMessage;

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
    Scene* settings_menu_scene;
    Scene* display_settings_scene;
    Scene* power_settings_scene;
    Scene* testing_menu_scene;
    Scene* leds_menu_scene;
    Scene* debug_menu_scene;

    FuriEventLoop* event_loop;
    DesktopApp app;
    FuriMessageQueue* app_message_queue;
    FuriMessageQueue* scene_event_message_queue;

    FuriEventLoopTimer* power_update_timer;
};

static void desktop_app_thread_state_callback(FuriThread* thread, FuriThreadState thread_state, void* context) {
    UNUSED(thread);
    furi_assert(context);

    if(thread_state == FuriThreadStateStopped) {
        Desktop* desktop = context;

        DesktopAppMessage message;
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

    DesktopAppMessage message;
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

void desktop_send_scene_event(Desktop* desktop, uint32_t event, void* data) {
    furi_check(desktop);

    DesktopSceneEventMessage message;
    message.event = event;
    message.data = data;

    furi_message_queue_put(desktop->scene_event_message_queue, &message, 0);
}

bool furi_crash_handler(bool debug) {
    return false; // Always false for this development stage
}

static void desktop_scene_event_logic(FuriEventLoopObject* object, void* context) {
    furi_check(context);
    Desktop* desktop = context;
    furi_check(object == desktop->scene_event_message_queue);

    DesktopSceneEventMessage message;
    furi_check(furi_message_queue_get(desktop->scene_event_message_queue, &message, 0) == FuriStatusOk);

    bool consumed = false;

    // TODO: think about a better way to handle this, maybe a scene stack or something
    switch(message.event) {
    case DesktopSceneEventTypeTogglePowerMenu:
        if(message.data) {
            scene_exit(desktop->power_menu_scene, desktop);
        } else {
            scene_enter(desktop->power_menu_scene, desktop);
        }
        consumed = true;
        break;
    case DesktopSceneEventTypeReturnToDesktop:
        if(message.data) scene_exit(message.data, desktop);
        scene_enter(desktop->main_scene, desktop);
        consumed = true;
        break;
    case DesktopSceneEventTypeEnterSettingsMenu:
        if(message.data) scene_exit(message.data, desktop);
        scene_enter(desktop->settings_menu_scene, desktop);
        consumed = true;
        break;
    case DesktopSceneEventTypeEnterDisplaySettings:
        if(message.data) scene_exit(message.data, desktop);
        scene_enter(desktop->display_settings_scene, desktop);
        consumed = true;
        break;
    case DesktopSceneEventTypeEnterPowerSettings:
        if(message.data) scene_exit(message.data, desktop);
        scene_enter(desktop->power_settings_scene, desktop);
        consumed = true;
        break;
    case DesktopSceneEventTypeEnterTestingMenu:
        if(message.data) scene_exit(message.data, desktop);
        scene_enter(desktop->testing_menu_scene, desktop);
        consumed = true;
        break;
    case DesktopSceneEventTypeEnterLedsMenu:
        if(message.data) scene_exit(message.data, desktop);
        scene_enter(desktop->leds_menu_scene, desktop);
        consumed = true;
        break;
    case DesktopSceneEventTypeOpenDebugMenu:
        scene_enter(desktop->debug_menu_scene, desktop);
        consumed = true;
        break;
    }

    if(!consumed) {
        consumed = scene_event(desktop->header_scene, message.event, message.data);
    }
}

static void desktop_power_update_timer_callback(void* context) {
    furi_check(context);
    Desktop* desktop = context;
    desktop_send_scene_event(desktop, DesktopSceneEventTypePowerUpdate, NULL);
}

static Desktop* desktop_alloc(void) {
    Desktop* desktop = malloc(sizeof(Desktop));
    desktop->gui = furi_record_open(RECORD_GUI);
    desktop->event_loop = furi_event_loop_alloc();
    desktop->app_message_queue = furi_message_queue_alloc(DESKTOP_APP_MESSAGE_QUEUE_SIZE, sizeof(DesktopAppMessage));
    desktop->scene_event_message_queue = furi_message_queue_alloc(DESKTOP_SCENE_EVENT_MESSAGE_QUEUE_SIZE, sizeof(DesktopSceneEventMessage));
    desktop->power_update_timer =
        furi_event_loop_timer_alloc(desktop->event_loop, desktop_power_update_timer_callback, FuriEventLoopTimerTypePeriodic, desktop);

    furi_event_loop_subscribe_message_queue(desktop->event_loop, desktop->app_message_queue, FuriEventLoopEventIn, desktop_app_message_logic, desktop);
    furi_event_loop_subscribe_message_queue(desktop->event_loop, desktop->scene_event_message_queue, FuriEventLoopEventIn, desktop_scene_event_logic, desktop);

    desktop->header_scene = scene_alloc(&scene_header_callbacks, desktop);
    desktop->main_scene = scene_alloc(&scene_desktop_callbacks, desktop);
    desktop->power_menu_scene = scene_alloc(&scene_power_menu_callbacks, desktop);
    desktop->settings_menu_scene = scene_alloc(&scene_settings_menu_callbacks, desktop);
    desktop->display_settings_scene = scene_alloc(&scene_display_settings_callbacks, desktop);
    desktop->power_settings_scene = scene_alloc(&scene_power_settings_callbacks, desktop);
    desktop->testing_menu_scene = scene_alloc(&scene_testing_menu_callbacks, desktop);
    desktop->leds_menu_scene = scene_alloc(&scene_leds_menu_callbacks, desktop);
    desktop->debug_menu_scene = scene_alloc(&scene_debug_menu_callbacks, desktop);

    gui_add_view(desktop->gui, scene_get_view(desktop->header_scene), GuiViewPriorityStatusBar);

    gui_add_view(desktop->gui, scene_get_view(desktop->main_scene), GuiViewPriorityDesktop);

    gui_add_view(desktop->gui, scene_get_view(desktop->settings_menu_scene), GuiViewPriorityDesktop);
    gui_add_view(desktop->gui, scene_get_view(desktop->display_settings_scene), GuiViewPriorityDesktop);
    gui_add_view(desktop->gui, scene_get_view(desktop->power_settings_scene), GuiViewPriorityDesktop);
    gui_add_view(desktop->gui, scene_get_view(desktop->testing_menu_scene), GuiViewPriorityDesktop);
    gui_add_view(desktop->gui, scene_get_view(desktop->leds_menu_scene), GuiViewPriorityDesktop);

    gui_add_view(desktop->gui, scene_get_view(desktop->debug_menu_scene), GuiViewPriorityApplication - 1);
    gui_add_view(desktop->gui, scene_get_view(desktop->power_menu_scene), GuiViewPriorityPowerMenu);

    scene_enter(desktop->header_scene, desktop);
    scene_enter(desktop->main_scene, desktop);

    furi_record_create(RECORD_DESKTOP, desktop);

    furi_event_loop_timer_start(desktop->power_update_timer, DESKTOP_POWER_UPDATE_PERIOD_MS);

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
    DesktopAppMessage message = {
        .type = DesktopMessageTypeAppStart,
        .app = app,
        .args = app->args,
    };

    furi_message_queue_put(desktop->app_message_queue, &message, FuriWaitForever);
    furi_record_close(RECORD_DESKTOP);

    return true;
}

extern int32_t cpu_app(void* p);

static const FlipperInternalApplication cpu_app_start = {
    .app = cpu_app,
    .name = "CPU App",
    .appid = "cpu",
    .stack_size = 1024 * 4,
    .flags = FlipperInternalApplicationFlagDefault,
    .args = "start",
};

static const FlipperInternalApplication cpu_app_maskrom = {
    .app = cpu_app,
    .name = "CPU App",
    .appid = "cpu",
    .stack_size = 1024 * 4,
    .flags = FlipperInternalApplicationFlagDefault,
    .args = "maskrom",
};

void desktop_start_cpu(bool to_maskrom) {
    desktop_start_app(to_maskrom ? &cpu_app_maskrom : &cpu_app_start);
}

void desktop_power_off(void) {
    Power* power_off = furi_record_open(RECORD_POWER);
    power_bq25792_set_power_switch(power_off, Bq25792PowerShipMode);
    furi_record_close(RECORD_POWER);
}
