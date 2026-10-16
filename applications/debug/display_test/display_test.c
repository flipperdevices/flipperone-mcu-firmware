#include <furi.h>
#include <gui/gui.h>
#include <gui/clay_helper.h>

#define TAG "DisplayTest"

typedef struct {
    uint32_t pattern_index;
} DisplayTestModel;

typedef struct {
    Gui* gui;
    View* view;
    FuriEventLoop* event_loop;
    FuriThread* thread;
} DisplayTest;

static bool display_test_layout(void* _model) {
    furi_assert(_model);
    DisplayTestModel* model = (DisplayTestModel*)_model;
    UNUSED(model);

    return false;
}

static bool display_test_input(InputEvent* event, void* context) {
    furi_check(context);
    DisplayTest* instance = context;
    bool consumed = false;

    if(event->key == InputKeyBack) {
        if(event->type == InputTypePress) {
            furi_thread_signal(instance->thread, FuriSignalExit, NULL);
            consumed = true;
        }
    }

    return consumed;
}

static bool display_test_input_touch(InputTouchEvent* event, void* context) {
    furi_check(context);
    DisplayTest* instance = context;
    UNUSED(event);
    UNUSED(instance);

    return false;
}

static DisplayTest* display_test_alloc(void) {
    DisplayTest* instance = malloc(sizeof(DisplayTest));
    instance->gui = furi_record_open(RECORD_GUI);
    instance->event_loop = furi_event_loop_alloc();
    instance->thread = furi_thread_get_current();

    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLockFree, sizeof(DisplayTestModel));
    view_set_layout_callback(instance->view, display_test_layout);
    view_set_input_callback(instance->view, display_test_input, instance);
    view_set_input_touch_callback(instance->view, display_test_input_touch, instance);
    gui_add_view(instance->gui, instance->view, GuiViewPriorityApplication);
    return instance;
}

static void display_test_free(DisplayTest* instance) {
    gui_remove_view(instance->gui, instance->view);
    furi_record_close(RECORD_GUI);
    view_free(instance->view);
    furi_event_loop_free(instance->event_loop);
    free(instance);
}

int32_t display_test_app(void* p) {
    UNUSED(p);
    DisplayTest* instance = display_test_alloc();
    furi_event_loop_run(instance->event_loop);
    display_test_free(instance);
    return 0;
}
