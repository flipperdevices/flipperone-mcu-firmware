#include <furi.h>
#include <gui/gui.h>
#include <gui/clay_helper.h>

#define TAG "TestApp"

typedef struct {
} AppModel;

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriEventLoop* event_loop;
    FuriThread* thread;
} App;

static bool app_layout(void* _model) {
    furi_assert(_model);
    AppModel* model = (AppModel*)_model;
}

static bool app_input(InputEvent* event, void* context) {
    furi_check(context);
    App* instance = context;
    bool consumed = false;

    if(event->key == InputKeyBack) {
        if(event->type == InputTypePress) {
            furi_thread_signal(instance->thread, FuriSignalExit, NULL);
            consumed = true;
        }
    }

    return consumed;
}

static bool app_input_touch(InputTouchEvent* event, void* context) {
    furi_check(context);
    App* instance = context;

    return false;
}

static App* app_alloc(void) {
    App* instance = malloc(sizeof(App));
    instance->gui = furi_record_open(RECORD_GUI);
    instance->event_loop = furi_event_loop_alloc();

    instance->view_port = view_port_alloc();
    view_port_allocate_model(instance->view_port, ViewPortModelTypeLockFree, sizeof(AppModel));
    view_port_set_layout_callback(instance->view_port, app_layout);
    view_port_set_input_callback(instance->view_port, app_input, instance);
    view_port_set_input_touch_callback(instance->view_port, app_input_touch, instance);
    gui_add_view_port(instance->gui, instance->view_port, GuiViewPriorityApplication);
    return instance;
}

static void app_free(App* instance) {
    gui_remove_view_port(instance->gui, instance->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(instance->view_port);
    furi_event_loop_free(instance->event_loop);
    free(instance);
}

int32_t test_app(void* p) {
    App* instance = app_alloc();
    furi_event_loop_run(instance->event_loop);
    app_free(instance);
    return 0;
}
