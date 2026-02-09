#include "gui.h"
#include "gui_i.h"
#include "view_port_i.h"
#include <m-array.h>
#include <m-algo.h>
#include "clay.h"
#include "clay_render.h"
#include <drivers/display/display_jd9853_qspi.h>
#include <drivers/display/display_jd9853_reg.h>

#define TAG "GuiSrv"

#define GUI_INPUT_EVENT_QUEUE_SIZE       32
#define GUI_INPUT_TOUCH_EVENT_QUEUE_SIZE 32

#define GUI_EVENT_FLAG_REDRAW (1U << 0)

typedef struct {
    ViewPort* view_port;
    GuiViewPriority priority;
} ViewPortHandle;

ARRAY_DEF(ViewPortArray, ViewPortHandle, M_POD_OPLIST);
#define M_OPL_ViewPortArray_t() ARRAY_OPLIST(ViewPortArray, M_POD_OPLIST)
ALGO_DEF(ViewPortArray, ViewPortArray_t);

/** Gui structure */
struct Gui {
    // Global gui mutex
    FuriMutex* mutex;

    // View ports
    ViewPortArray_t views;
    RenderBuffer* render_buffer;
    DisplayJd9853QSPI* display;

    // Event handling
    FuriEventLoop* event_loop;
    FuriEventFlag* redraw_flag;
    FuriMessageQueue* input_queue;
    FuriMessageQueue* input_touch_queue;
};

static int gui_view_port_compare(const ViewPortHandle* a, const ViewPortHandle* b) {
    if(a->priority < b->priority) return -1;
    if(a->priority > b->priority) return 1;
    return 0;
}

static bool gui_view_port_find_opaque_from_top(ViewPortArray_t array, ViewPortArray_it_t* it) {
    // Iterating backward
    ViewPortArray_it_last(*it, array);
    while(!ViewPortArray_end_p(*it)) {
        ViewPort* view_port = ViewPortArray_ref(*it)->view_port;
        if(view_port_is_enabled(view_port) && !view_port_is_transparent(view_port)) {
            return true;
        }
        ViewPortArray_previous(*it);
    }
    return false;
}

static bool gui_view_port_find_next_transparent(ViewPortArray_t array, ViewPortArray_it_t* it) {
    // Iterating forward
    while(!ViewPortArray_last_p(*it)) {
        ViewPortArray_next(*it);
        ViewPort* view_port = ViewPortArray_ref(*it)->view_port;
        if(view_port_is_enabled(view_port) && view_port_is_transparent(view_port)) {
            return true;
        }
    }
    return false;
}

static bool gui_view_port_find_any_from_top(ViewPortArray_t array, ViewPortArray_it_t* it) {
    // Iterating backward
    ViewPortArray_it_last(*it, array);
    while(!ViewPortArray_end_p(*it)) {
        ViewPort* view_port = ViewPortArray_ref(*it)->view_port;
        if(view_port_is_enabled(view_port)) {
            return true;
        }
        ViewPortArray_previous(*it);
    }
    return false;
}

static bool gui_view_port_find_any_previous(ViewPortArray_t array, ViewPortArray_it_t* it) {
    // Iterating backward
    while(!ViewPortArray_end_p(*it)) {
        ViewPortArray_previous(*it);
        ViewPort* view_port = ViewPortArray_ref(*it)->view_port;
        if(view_port_is_enabled(view_port)) {
            return true;
        }
    }
    return false;
}

static ViewPort* gui_view_port_from_it(ViewPortArray_it_t* it) {
    return ViewPortArray_ref(*it)->view_port;
}

void gui_update(Gui* gui) {
    furi_assert(gui);
    furi_event_flag_set(gui->redraw_flag, GUI_EVENT_FLAG_REDRAW);
}

static void gui_input_events_glue(const void* value, void* ctx) {
    furi_assert(value);
    furi_assert(ctx);
    furi_message_queue_put(ctx, value, FuriWaitForever);
}

static void gui_redraw(Gui* gui) {
    furi_assert(gui);
    gui_lock(gui);

    Clay_ResetMeasureTextCache();
    Clay_BeginLayout();

    ViewPortArray_it_t it;

    if(gui_view_port_find_opaque_from_top(gui->views, &it)) {
        do {
            view_port_layout(gui_view_port_from_it(&it));
        } while(gui_view_port_find_next_transparent(gui->views, &it));
    }

    Clay_RenderCommandArray renderCommands = Clay_EndLayout();

    render_do_render(&renderCommands);

    if(gui_view_port_find_opaque_from_top(gui->views, &it)) {
        do {
            view_port_post_layout(gui_view_port_from_it(&it));
        } while(gui_view_port_find_next_transparent(gui->views, &it));
    }

    size_t width = render_get_buffer_width(gui->render_buffer);
    size_t height = render_get_buffer_height(gui->render_buffer);
    display_jd9853_qspi_write_buffer(gui->display, render_get_buffer_data(gui->render_buffer), width * height);

    gui_unlock(gui);
}

static void gui_input_touch(Gui* gui, InputTouchEvent* input_event) {
    furi_assert(gui);
    furi_assert(input_event);

    gui_lock(gui);

    ViewPortArray_it_t it;
    if(gui_view_port_find_any_from_top(gui->views, &it)) {
        do {
            ViewPort* view_port = gui_view_port_from_it(&it);

            // Break if input was consumed
            if(view_port_input_touch(view_port, input_event)) break;

            // Break if view port is opaque
            if(!view_port_is_transparent(view_port)) break;
        } while(gui_view_port_find_any_previous(gui->views, &it));
    }

    gui_unlock(gui);
}

static void gui_input(Gui* gui, InputEvent* input_event) {
    furi_assert(gui);
    furi_assert(input_event);

    gui_lock(gui);

    ViewPortArray_it_t it;
    if(gui_view_port_find_any_from_top(gui->views, &it)) {
        do {
            ViewPort* view_port = gui_view_port_from_it(&it);

            // Break if input was consumed
            if(view_port_input(view_port, input_event)) break;

            // Break if view port is opaque
            if(!view_port_is_transparent(view_port)) break;
        } while(gui_view_port_find_any_previous(gui->views, &it));
    }

    gui_unlock(gui);
}

void gui_lock(Gui* gui) {
    furi_assert(gui);
    furi_check(furi_mutex_acquire(gui->mutex, FuriWaitForever) == FuriStatusOk);
}

void gui_unlock(Gui* gui) {
    furi_assert(gui);
    furi_check(furi_mutex_release(gui->mutex) == FuriStatusOk);
}

void gui_add_view_port(Gui* gui, ViewPort* view_port, GuiViewPriority priority) {
    furi_check(gui);
    furi_check(view_port);

    gui_lock(gui);

    // Verify that view port is not yet added
    ViewPortArray_it_t it;
    ViewPortArray_it(it, gui->views);
    while(!ViewPortArray_end_p(it)) {
        furi_assert(ViewPortArray_ref(it)->view_port != view_port);
        ViewPortArray_next(it);
    }

    // Add view port and link with gui
    ViewPortHandle handle = {.view_port = view_port, .priority = priority};
    ViewPortArray_push_back(gui->views, handle);
    view_port_gui_set(view_port, gui);

    // Sort view ports by priority
    ViewPortArray_special_sort(gui->views, gui_view_port_compare);

    gui_unlock(gui);

    // Request redraw
    gui_update(gui);
}

void gui_remove_view_port(Gui* gui, ViewPort* view_port) {
    furi_check(gui);
    furi_check(view_port);

    gui_lock(gui);
    view_port_gui_set(view_port, NULL);

    ViewPortArray_it_t it;
    ViewPortArray_it(it, gui->views);
    while(!ViewPortArray_end_p(it)) {
        if(ViewPortArray_ref(it)->view_port == view_port) {
            ViewPortArray_remove(gui->views, it);
        } else {
            ViewPortArray_next(it);
        }
    }

    gui_unlock(gui);

    // Request redraw
    gui_update(gui);
}

static void gui_handle_clay_errors(Clay_ErrorData errorData) {
    FURI_LOG_E(TAG, "clay error: %s", errorData.errorText.chars);
}

static void gui_input_logic(FuriEventLoopObject* object, void* context) {
    furi_check(context);
    Gui* gui = context;
    furi_check(object == gui->input_queue);

    InputEvent input_event;
    while(furi_message_queue_get(gui->input_queue, &input_event, 0) == FuriStatusOk) {
        gui_input(gui, &input_event);
    }
}

static void gui_input_touch_logic(FuriEventLoopObject* object, void* context) {
    furi_check(context);
    Gui* gui = context;
    furi_check(object == gui->input_touch_queue);

    InputTouchEvent input_touch_event;
    while(furi_message_queue_get(gui->input_touch_queue, &input_touch_event, 0) == FuriStatusOk) {
        gui_input_touch(gui, &input_touch_event);
    }
}

static void gui_redraw_logic(FuriEventLoopObject* object, void* context) {
    furi_check(context);
    Gui* gui = context;
    furi_check(object == gui->redraw_flag);
    furi_event_flag_clear(gui->redraw_flag, GUI_EVENT_FLAG_REDRAW);
    gui_redraw(gui);
}

static Gui* gui_alloc(void) {
    Gui* gui = malloc(sizeof(Gui));

    // Allocate mutex
    gui->mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    // Event handling
    gui->event_loop = furi_event_loop_alloc();
    gui->redraw_flag = furi_event_flag_alloc();
    gui->input_queue = furi_message_queue_alloc(GUI_INPUT_EVENT_QUEUE_SIZE, sizeof(InputEvent));
    gui->input_touch_queue = furi_message_queue_alloc(GUI_INPUT_TOUCH_EVENT_QUEUE_SIZE, sizeof(InputTouchEvent));

    // View ports
    ViewPortArray_init(gui->views);

    // Display and buffer
    gui->display = display_jd9853_qspi_init();
    display_jd9853_qspi_set_brightness(gui->display, 20);
    gui->render_buffer = render_alloc_buffer();
    render_set_current_buffer(gui->render_buffer);

    // Clay initialization
    Clay_SetMaxElementCount(128);
    Clay_SetMaxMeasureTextCacheWordCount(512);
    uint64_t totalMemorySize = Clay_MinMemorySize();
    FURI_LOG_I(TAG, "Clay allocation: %lluk", totalMemorySize / 1024);
    Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(totalMemorySize, malloc(totalMemorySize));
    Clay_Initialize(arena, (Clay_Dimensions){JD9853_WIDTH, JD9853_HEIGHT}, (Clay_ErrorHandler){gui_handle_clay_errors, gui});
    Clay_SetMeasureTextFunction(render_measure_text, NULL);

    // Subscribe to input events
    furi_pubsub_subscribe(furi_record_open(RECORD_INPUT_EVENTS), gui_input_events_glue, gui->input_queue);
    furi_pubsub_subscribe(furi_record_open(RECORD_INPUT_TOUCH_EVENTS), gui_input_events_glue, gui->input_touch_queue);

    // Event loop subscriptions
    furi_event_loop_subscribe_message_queue(gui->event_loop, gui->input_queue, FuriEventLoopEventIn, gui_input_logic, gui);
    furi_event_loop_subscribe_message_queue(gui->event_loop, gui->input_touch_queue, FuriEventLoopEventIn, gui_input_touch_logic, gui);
    furi_event_loop_subscribe_event_flag(gui->event_loop, gui->redraw_flag, FuriEventLoopEventIn, gui_redraw_logic, gui);

    return gui;
}

int32_t gui_srv(void* p) {
    UNUSED(p);
    Gui* gui = gui_alloc();

    furi_record_create(RECORD_GUI, gui);
    furi_event_loop_run(gui->event_loop);

    return 0;
}
