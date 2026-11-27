#include <gui/gui.h>
#include <haptic/haptic.h>
#include <desktop/desktop.h>
#include "self_check_screen.h"

#define TAG "SelfCheck"

typedef struct {
    const SelfCheckScreen* screen;
} SelfCheckModel;

typedef struct {
    Gui* gui;
    View* view;
    FuriEventLoop* event_loop;
    FuriThread* thread;
    SelfCheckScreen* screen;
} SelfCheck;

static bool self_check_layout(void* _model) {
    furi_assert(_model);
    SelfCheckModel* model = (SelfCheckModel*)_model;
    self_check_screen_layout(model->screen, "Self Check");
    return false;
}

static bool self_check_input(InputEvent* event, void* context) {
    furi_check(context);
    SelfCheck* instance = context;
    bool consumed = false;

    if(event->type == InputTypePress) {
        if(event->key == InputKeyOk || event->key == InputKeyBack) {
            furi_thread_signal(instance->thread, FuriSignalExit, NULL);
            consumed = true;
        }
    }
    return consumed;
}

static bool self_check_input_touch(InputTouchEvent* event, void* context) {
    furi_check(context);
    SelfCheck* instance = context;
    UNUSED(event);
    UNUSED(instance);

    return false;
}

static SelfCheck* self_check_alloc(void) {
    SelfCheck* instance = malloc(sizeof(SelfCheck));
    instance->gui = furi_record_open(RECORD_GUI);
    instance->event_loop = furi_event_loop_alloc();
    instance->thread = furi_thread_get_current();
    instance->view = view_alloc();

    instance->screen = self_check_screen_alloc();
    if(!self_check_screen_is_ok(instance->screen)) {
        Haptic* haptic = furi_record_open(RECORD_HAPTIC);
        haptic_play_effect(haptic, Drv2605lEffectDoubleClick_100, 0);
        furi_record_close(RECORD_HAPTIC);
    }

    view_allocate_model(instance->view, ViewModelTypeLockFree, sizeof(SelfCheckModel));
    with_view_model(instance->view, SelfCheckModel * model, { model->screen = instance->screen; }, false);

    view_set_layout_callback(instance->view, self_check_layout);
    view_set_input_callback(instance->view, self_check_input, instance);
    view_set_input_touch_callback(instance->view, self_check_input_touch, instance);
    gui_add_view(instance->gui, instance->view, GuiViewPriorityApplication);
    return instance;
}

static void self_check_free(SelfCheck* instance) {
    gui_remove_view(instance->gui, instance->view);
    furi_record_close(RECORD_GUI);
    self_check_screen_free(instance->screen);
    view_free(instance->view);
    furi_event_loop_free(instance->event_loop);
    free(instance);
}

static void self_check_app_main(void) {
    SelfCheck* instance = self_check_alloc();
    furi_event_loop_run(instance->event_loop);
    self_check_free(instance);
}

static void self_check_app_autorun(void) {
    FURI_LOG_I(TAG, "Starting self check autorun");

    // Occupy the desktop app slot right away: no other app can be started
    // via desktop while we are running (and desktop can stop us on demand).
    // A taken slot is the host's screen, started for a host still powered
    // across an MCU reset: it wins over the check, as it does once running.
    if(!desktop_register_app("self_check", furi_thread_get_current())) {
        FURI_LOG_W(TAG, "Desktop slot taken, skipping self check");
        return;
    }

    SelfCheckScreen* screen = self_check_screen_alloc();
    const bool ok = self_check_screen_is_ok(screen);
    self_check_screen_free(screen);
    if(!ok) {
        self_check_app_main();
    }

    desktop_unregister_app("self_check");
}

int32_t self_check_app(void* p) {
    if(p && strcmp((char*)p, "autorun") == 0) {
        self_check_app_autorun();
    } else {
        self_check_app_main();
    }

    return 0;
}
