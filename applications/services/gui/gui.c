#include "gui.h"
#include "gui_i.h"
#include "view_i.h"
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
    View* view;
    GuiViewPriority priority;
} ViewHandle;

ARRAY_DEF(ViewHandleArray, ViewHandle, M_POD_OPLIST);
#define M_OPL_ViewHandleArray_t() ARRAY_OPLIST(ViewHandleArray, M_POD_OPLIST)
ALGO_DEF(ViewHandleArray, ViewHandleArray_t);

/** Gui structure */
struct Gui {
    // Global gui mutex
    FuriMutex* mutex;

    // View ports
    ViewHandleArray_t views;
    Canvas* render_canvas;
    DisplayJd9853QSPI* display;

    // Event handling
    FuriEventLoop* event_loop;
    FuriEventFlag* redraw_flag;
    FuriMessageQueue* input_queue;
    FuriMessageQueue* input_touch_queue;
};

static int gui_view_compare(const ViewHandle* a, const ViewHandle* b) {
    if(a->priority < b->priority) return -1;
    if(a->priority > b->priority) return 1;
    return 0;
}

static bool gui_view_find_opaque_from_top(ViewHandleArray_t array, ViewHandleArray_it_t* it) {
    // Iterating backward
    ViewHandleArray_it_last(*it, array);
    while(!ViewHandleArray_end_p(*it)) {
        View* view = ViewHandleArray_ref(*it)->view;
        if(view_is_enabled(view) && !view_is_transparent(view)) {
            return true;
        }
        ViewHandleArray_previous(*it);
    }
    return false;
}

static bool gui_view_find_next_transparent(ViewHandleArray_it_t* it) {
    // Iterating forward
    while(!ViewHandleArray_last_p(*it)) {
        ViewHandleArray_next(*it);
        View* view = ViewHandleArray_ref(*it)->view;
        if(view_is_enabled(view) && view_is_transparent(view)) {
            return true;
        }
    }
    return false;
}

static bool gui_view_find_any_from_top(ViewHandleArray_t array, ViewHandleArray_it_t* it) {
    // Iterating backward
    ViewHandleArray_it_last(*it, array);
    while(!ViewHandleArray_end_p(*it)) {
        View* view = ViewHandleArray_ref(*it)->view;
        if(view_is_enabled(view)) {
            return true;
        }
        ViewHandleArray_previous(*it);
    }
    return false;
}

static bool gui_view_find_any_previous(ViewHandleArray_it_t* it) {
    // Iterating backward
    while(!ViewHandleArray_end_p(*it)) {
        ViewHandleArray_previous(*it);
        View* view = ViewHandleArray_ref(*it)->view;
        if(view_is_enabled(view)) {
            return true;
        }
    }
    return false;
}

static View* gui_view_from_it(ViewHandleArray_it_t* it) {
    return ViewHandleArray_ref(*it)->view;
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

    ViewHandleArray_it_t it;

    CLAY(
        CLAY_ID("GUI"),
        {
            .layout =
                {
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                    // workaround for the pixel shift bug
                    .padding = {.left = 1, .top = 0, .right = 0, .bottom = 0},
                },
        }) {
        if(gui_view_find_opaque_from_top(gui->views, &it)) {
            do {
                view_layout(gui_view_from_it(&it));
            } while(gui_view_find_next_transparent(&it));
        }
    }

    Clay_RenderCommandArray renderCommands = Clay_EndLayout();

    render_do_render(gui->render_canvas, &renderCommands);

    if(gui_view_find_opaque_from_top(gui->views, &it)) {
        do {
            view_post_layout(gui_view_from_it(&it));
        } while(gui_view_find_next_transparent(&it));
    }

    size_t width = canvas_get_width(gui->render_canvas);
    size_t height = canvas_get_height(gui->render_canvas);
    display_jd9853_qspi_write_buffer(gui->display, canvas_get_data(gui->render_canvas), width * height);

    gui_unlock(gui);
}

static void gui_input_touch(Gui* gui, InputTouchEvent* input_event) {
    furi_assert(gui);
    furi_assert(input_event);

    gui_lock(gui);

    ViewHandleArray_it_t it;
    if(gui_view_find_any_from_top(gui->views, &it)) {
        do {
            View* view = gui_view_from_it(&it);

            // Break if input was consumed
            if(view_input_touch(view, input_event)) break;

            // Break if view port is opaque
            if(!view_is_transparent(view)) break;
        } while(gui_view_find_any_previous(&it));
    }

    gui_unlock(gui);
}

static void gui_input(Gui* gui, InputEvent* input_event) {
    furi_assert(gui);
    furi_assert(input_event);

    gui_lock(gui);

    ViewHandleArray_it_t it;
    if(gui_view_find_any_from_top(gui->views, &it)) {
        do {
            View* view = gui_view_from_it(&it);

            // Break if input was consumed
            if(view_input(view, input_event)) break;

            // Break if view port is opaque
            if(!view_is_transparent(view)) break;
        } while(gui_view_find_any_previous(&it));
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

void gui_add_view(Gui* gui, View* view, GuiViewPriority priority) {
    furi_check(gui);
    furi_check(view);

    gui_lock(gui);

    // Verify that view port is not yet added
    ViewHandleArray_it_t it;
    ViewHandleArray_it(it, gui->views);
    while(!ViewHandleArray_end_p(it)) {
        furi_assert(ViewHandleArray_ref(it)->view != view);
        ViewHandleArray_next(it);
    }

    // Add view port and link with gui
    ViewHandle handle = {.view = view, .priority = priority};
    ViewHandleArray_push_back(gui->views, handle);
    view_gui_set(view, gui);

    // Sort view ports by priority
    ViewHandleArray_special_sort(gui->views, gui_view_compare);

    gui_unlock(gui);

    // Request redraw
    gui_update(gui);
}

void gui_remove_view(Gui* gui, View* view) {
    furi_check(gui);
    furi_check(view);

    gui_lock(gui);
    view_gui_set(view, NULL);

    ViewHandleArray_it_t it;
    ViewHandleArray_it(it, gui->views);
    while(!ViewHandleArray_end_p(it)) {
        if(ViewHandleArray_ref(it)->view == view) {
            ViewHandleArray_remove(gui->views, it);
        } else {
            ViewHandleArray_next(it);
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

void gui_set_backlight(Gui* gui, int8_t brightness) {
    furi_check(gui);
    display_jd9853_qspi_set_brightness(gui->display, brightness);
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
    ViewHandleArray_init(gui->views);

    // Display and buffer
    gui->display = display_jd9853_qspi_init();
    gui_set_backlight(gui, 20);
    gui->render_canvas = canvas_alloc(JD9853_WIDTH, JD9853_HEIGHT);

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
