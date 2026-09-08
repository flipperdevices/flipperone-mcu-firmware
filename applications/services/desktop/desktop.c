#include "desktop.h"
#include "desktop_i.h"
#include <applications.h>
#include <power/power.h>
#include <gui/clay_helper.h>
#include <gui/gui.h>
#include <assets.h>
#include <api_lock.h>
#include "scenes/scenes.h"

#define DESKTOP_APP_MESSAGE_QUEUE_SIZE         4
#define DESKTOP_SCENE_EVENT_MESSAGE_QUEUE_SIZE 16

#define DESKTOP_POWER_UPDATE_PERIOD_MS 5000

#define TAG "DesktopSrv"

#define DESKTOP_MENU_ID(x) CLAY_SIDI(CLAY_STRING("DesktopMenu"), x)

typedef enum {
    DesktopMessageTypeAppStart,
    DesktopMessageTypeAppStop,
    DesktopMessageTypeAppClosed,
    DesktopMessageTypeAppRegister,
    DesktopMessageTypeAppUnregister,
    DesktopMessageTypeAppCleanupWorkerDone,
} DesktopMessageType;

typedef struct {
    DesktopMessageType type;
    const FlipperInternalApplication* app;
    const char* args;
    const char* appid;
    FuriThread* thread;
    /* Only used by Register/Unregister so the caller can block until the
     * desktop thread has applied the state change and get the real result,
     * instead of racing with it by reading desktop->app.* directly. */
    FuriApiLock lock;
    bool* result;
} DesktopAppMessage;

typedef struct {
    uint32_t event;
    void* data;
} DesktopSceneEventMessage;

typedef struct {
    bool running;
    bool external;
    char* args;
    FuriThread* thread;
    const char* name;
    const char* appid;
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

static void desktop_send_message(Desktop* instance, const DesktopAppMessage* message);

static void desktop_app_thread_state_callback(FuriThread* thread, FuriThreadState thread_state, void* context) {
    UNUSED(thread);
    furi_assert(context);

    if(thread_state == FuriThreadStateStopped) {
        Desktop* desktop = context;

        DesktopAppMessage message = {
            .type = DesktopMessageTypeAppClosed,
        };
        desktop_send_message(desktop, &message);
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
    desktop->app.name = NULL;
    desktop->app.appid = NULL;

    FURI_LOG_I(TAG, "Application stopped. Free heap: %zu", memmgr_get_free_heap());
}

/* Join/free of an external app thread must happen off both the desktop event
 * loop (the app calls desktop_unregister_app() from its own thread before it
 * has fully stopped, so joining there would stall desktop's own message
 * processing) and the shared FreeRTOS timer daemon (furi_thread_join() polls
 * until the thread stops, which would stall every other system timer and
 * pending callback in the firmware).
 *
 * Instead, a short-lived worker thread is spawned on demand to do the join;
 * once it exits, its own thread-stopped callback (which fires with the state
 * already set to Stopped, so joining it back is instant) posts a message so
 * the desktop thread can reap it. No thread/resources linger when idle. */
typedef struct {
    FuriThread* thread;
    char* args;
} DesktopAppCleanupContext;

static int32_t desktop_app_cleanup_worker(void* context) {
    DesktopAppCleanupContext* cleanup = context;

    furi_thread_join(cleanup->thread);
    FURI_LOG_I(TAG, "App returned: %li", furi_thread_get_return_code(cleanup->thread));

    if(cleanup->args) {
        free(cleanup->args);
    }

    furi_thread_free(cleanup->thread);
    free(cleanup);

    FURI_LOG_I(TAG, "Application stopped. Free heap: %zu", memmgr_get_free_heap());

    return 0;
}

static void desktop_app_cleanup_worker_state_callback(FuriThread* thread, FuriThreadState thread_state, void* context) {
    furi_assert(context);

    if(thread_state == FuriThreadStateStopped) {
        Desktop* desktop = context;

        DesktopAppMessage message = {
            .type = DesktopMessageTypeAppCleanupWorkerDone,
            .thread = thread,
        };
        desktop_send_message(desktop, &message);
    }
}

static bool desktop_is_known_appid(const char* appid) {
    for(size_t i = 0; i < FLIPPER_APPS_COUNT; i++) {
        if(strcmp(FLIPPER_APPS[i].appid, appid) == 0) return true;
    }
    for(size_t i = 0; i < FLIPPER_AUTORUN_APPS_COUNT; i++) {
        if(strcmp(FLIPPER_AUTORUN_APPS[i].appid, appid) == 0) return true;
    }
    return false;
}

static void desktop_app_message_logic(FuriEventLoopObject* object, void* context) {
    furi_check(context);
    Desktop* desktop = context;
    furi_check(object == desktop->app_message_queue);

    DesktopAppMessage message;
    furi_check(furi_message_queue_get(desktop->app_message_queue, &message, 0) == FuriStatusOk);

    bool result = false;

    switch(message.type) {
    case DesktopMessageTypeAppStart:
        if(desktop->app.running) {
            FURI_LOG_E(TAG, "App start requested for %s, but %s is already running", message.app->appid, desktop->app.name);
        } else {
            desktop->app.running = true;
            desktop->app.external = false;
            desktop->app.name = message.app->name;
            desktop->app.appid = message.app->appid;
            desktop_start_internal_app(desktop, message.app, message.args);
            result = true;
        }
        break;
    case DesktopMessageTypeAppStop:
        if(desktop->app.running) {
            FURI_LOG_I(TAG, "App stop requested, sending exit signal");
            if(!furi_thread_signal(desktop->app.thread, FuriSignalExit, NULL)) {
                FURI_LOG_W(TAG, "App did not consume the exit signal");
            }
        }
        break;
    case DesktopMessageTypeAppClosed:
        furi_check(desktop->app.running);
        desktop_do_app_closed(desktop);
        desktop->app.running = false;
        desktop->app.external = false;
        break;
    case DesktopMessageTypeAppRegister:
        if(desktop->app.running) {
            FURI_LOG_E(TAG, "App register requested, but another app is already running: %s", message.appid);
        } else if(!desktop_is_known_appid(message.appid)) {
            FURI_LOG_E(TAG, "App register requested for unknown appid: %s", message.appid);
        } else if(strcmp(furi_thread_get_appid(message.thread), message.appid) != 0) {
            FURI_LOG_E(TAG, "App register requested for %s, but thread appid is %s", message.appid, furi_thread_get_appid(message.thread));
        } else {
            desktop->app.running = true;
            desktop->app.external = true;
            desktop->app.thread = message.thread;
            desktop->app.name = message.appid;
            desktop->app.appid = message.appid;
            FURI_LOG_I(TAG, "App registered as running: %s", message.appid);
            result = true;
        }
        break;
    case DesktopMessageTypeAppUnregister:
        if(desktop->app.running && desktop->app.external) {
            if(strcmp(desktop->app.appid, message.appid) != 0) {
                FURI_LOG_W(TAG, "Unregister appid mismatch: registered %s, requested %s", desktop->app.appid, message.appid);
            } else {
                FURI_LOG_I(TAG, "Registered app exiting: %s", message.appid);

                DesktopAppCleanupContext* cleanup = malloc(sizeof(DesktopAppCleanupContext));
                cleanup->thread = desktop->app.thread;
                cleanup->args = desktop->app.args;

                desktop->app.thread = NULL;
                desktop->app.args = NULL;
                desktop->app.running = false;
                desktop->app.external = false;
                desktop->app.name = NULL;
                desktop->app.appid = NULL;

                FuriThread* worker = furi_thread_alloc_ex("DesktopAppCleanup", 1024, desktop_app_cleanup_worker, cleanup);
                furi_thread_set_state_context(worker, desktop);
                furi_thread_set_state_callback(worker, desktop_app_cleanup_worker_state_callback);
                furi_thread_start(worker);

                result = true;
            }
        } else if(desktop->app.running) {
            FURI_LOG_W(TAG, "Unregister requested for desktop-managed app %s, ignoring", message.appid);
        } else {
            FURI_LOG_W(TAG, "Unregister requested for %s, but no app is running", message.appid);
        }
        break;
    case DesktopMessageTypeAppCleanupWorkerDone:
        // State is already Stopped by the time this fires, so this join is instant.
        furi_thread_join(message.thread);
        furi_thread_free(message.thread);
        break;
    default:
        furi_assert(false);
        break;
    }

    if(message.result) {
        *message.result = result;
    }

    if(message.lock) {
        api_lock_unlock(message.lock);
    }
}

void desktop_send_scene_event(Desktop* desktop, uint32_t event, void* data) {
    furi_check(desktop);

    DesktopSceneEventMessage message;
    message.event = event;
    message.data = data;

    furi_check(furi_message_queue_put(desktop->scene_event_message_queue, &message, 0) == FuriStatusOk);
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
    case DesktopSceneEventTypeEnterSelfCheckApp:
        desktop_start_app_by_id(desktop, "self_check");
        consumed = true;
        break;
    case DesktopSceneEventTypeEnterMaskromApp:
        desktop_start_app_by_id(desktop, "cpu_app_maskrom");
        consumed = true;
        break;
    case DesktopSceneEventTypeEnterCpuStartApp:
        desktop_start_app_by_id(desktop, "cpu_app_start");
        consumed = true;
        break;
    case DesktopSceneEventTypeEnterKeypadApp:
        desktop_start_app_by_id(desktop, "keypad_test");
        consumed = true;
        break;
    case DesktopSceneEventTypeEnterTouchpadApp:
        desktop_start_app_by_id(desktop, "touchpad_test");
        consumed = true;
        break;
    case DesktopSceneEventTypeEnterHapticApp:
        desktop_start_app_by_id(desktop, "haptic_test");
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

    desktop->app.running = false;
    desktop->app.external = false;

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

static void desktop_send_message(Desktop* instance, const DesktopAppMessage* message) {
    furi_check(furi_message_queue_put(instance->app_message_queue, message, FuriWaitForever) == FuriStatusOk);

    if(message->lock) {
        api_lock_wait_unlock_and_free(message->lock);
    }
}

bool desktop_start_app(const FlipperInternalApplication* app) {
    furi_assert(app);

    Desktop* desktop = furi_record_open(RECORD_DESKTOP);

    bool result = false;
    DesktopAppMessage message = {
        .type = DesktopMessageTypeAppStart,
        .app = app,
        .args = app->args,
        .lock = api_lock_alloc_locked(),
        .result = &result,
    };

    desktop_send_message(desktop, &message);
    furi_record_close(RECORD_DESKTOP);

    return result;
}

const char* desktop_get_running_app_name(void) {
    Desktop* desktop = furi_record_open(RECORD_DESKTOP);
    const char* name = desktop->app.running ? desktop->app.name : NULL;
    furi_record_close(RECORD_DESKTOP);

    return name;
}

const char* desktop_get_running_app_id(void) {
    Desktop* desktop = furi_record_open(RECORD_DESKTOP);
    const char* appid = desktop->app.running ? desktop->app.appid : NULL;
    furi_record_close(RECORD_DESKTOP);

    return appid;
}

bool desktop_stop_app(void) {
    Desktop* desktop = furi_record_open(RECORD_DESKTOP);

    bool running = desktop->app.running;

    DesktopAppMessage message = {
        .type = DesktopMessageTypeAppStop,
    };
    desktop_send_message(desktop, &message);
    furi_record_close(RECORD_DESKTOP);

    return running;
}

bool desktop_register_app(const char* appid, FuriThread* thread) {
    furi_check(appid);
    furi_check(thread);

    Desktop* desktop = furi_record_open(RECORD_DESKTOP);

    bool result = false;
    DesktopAppMessage message = {
        .type = DesktopMessageTypeAppRegister,
        .appid = appid,
        .thread = thread,
        .lock = api_lock_alloc_locked(),
        .result = &result,
    };
    desktop_send_message(desktop, &message);
    furi_record_close(RECORD_DESKTOP);

    return result;
}

bool desktop_unregister_app(const char* appid) {
    furi_check(appid);

    Desktop* desktop = furi_record_open(RECORD_DESKTOP);

    bool result = false;
    DesktopAppMessage message = {
        .type = DesktopMessageTypeAppUnregister,
        .appid = appid,
        .lock = api_lock_alloc_locked(),
        .result = &result,
    };
    desktop_send_message(desktop, &message);
    furi_record_close(RECORD_DESKTOP);

    return result;
}

bool desktop_start_app_by_id(Desktop* desktop, const char* appid) {
    furi_assert(appid);
    furi_assert(desktop);

    bool result = false;

    const FlipperInternalApplication* entry = NULL;
    for(size_t i = 0; i < FLIPPER_APPS_COUNT; i++) {
        if(strcmp(FLIPPER_APPS[i].appid, appid) == 0) {
            entry = &FLIPPER_APPS[i];
            break;
        }
    }
    do {
        if(!entry) {
            FURI_LOG_E(TAG, "App not found in FLIPPER_APPS: %s", appid);
            break;
        }

        if(desktop->app.running) {
            FURI_LOG_E(TAG, "App start requested for %s, but %s is already running", entry->appid, desktop->app.name);
        } else {
            desktop->app.running = true;
            desktop->app.external = false;
            desktop->app.name = entry->name;
            desktop->app.appid = entry->appid;
            desktop_start_internal_app(desktop, entry, entry->args);
            result = true;
        };

    } while(false);

    return result;
}

void desktop_power_off(void) {
    Power* power_off = furi_record_open(RECORD_POWER);
    power_bq2579x_set_power_switch(power_off, Bq2579xPowerShipMode);
    furi_record_close(RECORD_POWER);
}
