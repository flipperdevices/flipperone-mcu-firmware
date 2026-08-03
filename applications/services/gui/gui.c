#include "gui.h"
#include "gui_i.h"
#include "view_i.h"
#include <m-array.h>
#include <m-algo.h>
#include "clay.h"
#include "clay_render.h"
#include "clay_helper.h"
#include <drivers/display/display_jd9853_qspi.h>
#include <drivers/display/display_jd9853_reg.h>
#include <string.h>

#define TAG "GuiSrv"

#define GUI_INPUT_EVENT_QUEUE_SIZE       32
#define GUI_INPUT_TOUCH_EVENT_QUEUE_SIZE 32

#define GUI_EVENT_FLAG_REDRAW (1U << 0)

#define CLAY_MAX_ELEMENT_COUNT            128
#define CLAY_MAX_MEASURE_TEXT_CACHE_WORDS 256

typedef struct {
    View* view;
    GuiViewPriority priority;
} ViewHandle;

ARRAY_DEF(ViewHandleArray, ViewHandle, M_POD_OPLIST);
#define M_OPL_ViewHandleArray_t() ARRAY_OPLIST(ViewHandleArray, M_POD_OPLIST)
ALGO_DEF(ViewHandleArray, ViewHandleArray_t);

ARRAY_DEF(GuiCallbackArray, GuiCallbackPair, M_POD_OPLIST);
#define M_OPL_GuiCallbackArray_t() ARRAY_OPLIST(GuiCallbackArray, M_POD_OPLIST)
ALGO_DEF(GuiCallbackArray, GuiCallbackArray_t);

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

    // Unhandled input callbacks
    struct {
        ViewInputCallback input_callback;
        void* input_context;
        ViewInputTouchCallback input_touch_callback;
        void* input_touch_context;
    } unhandled;

    FuriMutex* callback_mutex;
    GuiCallbackArray_t gui_callbacks_pair;
};

static int gui_view_compare(const ViewHandle* a, const ViewHandle* b) {
    if(a->priority < b->priority) return -1;
    if(a->priority > b->priority) return 1;
    return 0;
}

static bool gui_view_find_opaque_from_top(ViewHandleArray_t array, ViewHandleArray_it_t* it) {
    ViewHandleArray_it_last(*it, array);

    // Iterating backward
    while(!ViewHandleArray_end_p(*it)) {
        ViewHandle* handle = ViewHandleArray_ref(*it);

        // Root layer: draw everything
        if(handle->priority < 100) {
            ViewHandleArray_it(*it, array);
            return true;
        }
        if(view_is_enabled(handle->view) && (!view_is_transparent(handle->view))) {
            // ViewHandleArray_next(*it);
            return true;
        }

        ViewHandleArray_previous(*it);
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
                    .padding = {.left = 1, .top = 0, .right = 1, .bottom = 0},
                },
        }) {
        if(gui_view_find_opaque_from_top(gui->views, &it)) {
            do {
                ViewHandle* handle = ViewHandleArray_ref(it);
                View* view = gui_view_from_it(&it);
                if(view_is_enabled(view)) {
                    view_layout(view);
                }
                ViewHandleArray_next(it);
            } while(!ViewHandleArray_end_p(it));
        }
    }

    Clay_RenderCommandArray renderCommands = Clay_EndLayout();

    clay_render_do_render(gui->render_canvas, &renderCommands);

    if(gui_view_find_opaque_from_top(gui->views, &it)) {
        do {
            ViewHandle* handle = ViewHandleArray_ref(it);
            View* view = gui_view_from_it(&it);
            if(view_is_enabled(view)) view_post_layout(view);
            ViewHandleArray_next(it);
        } while(!ViewHandleArray_end_p(it));
    }

    size_t width = canvas_get_width(gui->render_canvas);
    size_t height = canvas_get_height(gui->render_canvas);
    display_jd9853_qspi_write_buffer(gui->display, canvas_get_data(gui->render_canvas), width * height);

    furi_check(furi_mutex_acquire(gui->callback_mutex, FuriWaitForever) == FuriStatusOk);
    for
        M_EACH(p, gui->gui_callbacks_pair, GuiCallbackArray_t) {
            p->callback(canvas_get_data(gui->render_canvas), width, height, p->context);
        }
    furi_mutex_release(gui->callback_mutex);

    gui_unlock(gui);
}

static void gui_input_touch(Gui* gui, InputTouchEvent* input_event) {
    furi_assert(gui);
    furi_assert(input_event);

    gui_lock(gui);

    bool consumed = false;
    ViewHandleArray_it_t it;
    if(gui_view_find_any_from_top(gui->views, &it)) {
        do {
            View* view = gui_view_from_it(&it);

            // Break if input was consumed
            if(view_input_touch(view, input_event)) {
                consumed = true;
                break;
            }

            // Break if view port is opaque
            if(!view_is_transparent(view)) break;

        } while(gui_view_find_any_previous(&it));
    }

    // If input was not consumed by any view, send to unhandled callback
    if(!consumed && gui->unhandled.input_touch_callback) {
        gui->unhandled.input_touch_callback(input_event, gui->unhandled.input_touch_context);
    }

    gui_unlock(gui);
}

static void gui_input(Gui* gui, InputEvent* input_event) {
    furi_assert(gui);
    furi_assert(input_event);

    gui_lock(gui);

    bool consumed = false;
    ViewHandleArray_it_t it;
    if(gui_view_find_any_from_top(gui->views, &it)) {
        do {
            View* view = gui_view_from_it(&it);

            // Break if input was consumed
            if(view_input(view, input_event)) {
                consumed = true;
                break;
            }

            // Break if view port is opaque
            if(!view_is_transparent(view)) break;
        } while(gui_view_find_any_previous(&it));
    }

    // If input was not consumed by any view, send to unhandled callback
    if(!consumed && gui->unhandled.input_callback) {
        gui->unhandled.input_callback(input_event, gui->unhandled.input_context);
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

void gui_add_unhandled_input_callback(Gui* gui, ViewInputCallback callback, void* context) {
    gui_lock(gui);
    gui->unhandled.input_callback = callback;
    gui->unhandled.input_context = context;
    gui_unlock(gui);
}

void gui_add_unhandled_touch_input_callback(Gui* gui, ViewInputTouchCallback callback, void* context) {
    gui_lock(gui);
    gui->unhandled.input_touch_callback = callback;
    gui->unhandled.input_touch_context = context;
    gui_unlock(gui);
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

static Gui* gui_alloc(void) {
    canvas_init();

    Gui* gui = malloc(sizeof(Gui));
    FURI_LOG_I(TAG, "Gui struct: %zu bytes", sizeof(Gui));

    // Allocate mutex
    gui->mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    // Event handling
    gui->event_loop = furi_event_loop_alloc();
    gui->redraw_flag = furi_event_flag_alloc();
    gui->input_queue = furi_message_queue_alloc(GUI_INPUT_EVENT_QUEUE_SIZE, sizeof(InputEvent));
    FURI_LOG_I(
        TAG, "InputEvent: %zu bytes x %u → queue ~%zu bytes", sizeof(InputEvent), GUI_INPUT_EVENT_QUEUE_SIZE, sizeof(InputEvent) * GUI_INPUT_EVENT_QUEUE_SIZE);
    gui->input_touch_queue = furi_message_queue_alloc(GUI_INPUT_TOUCH_EVENT_QUEUE_SIZE, sizeof(InputTouchEvent));
    FURI_LOG_I(
        TAG,
        "InputTouchEvent: %zu bytes x %u → queue ~%zu bytes",
        sizeof(InputTouchEvent),
        GUI_INPUT_TOUCH_EVENT_QUEUE_SIZE,
        sizeof(InputTouchEvent) * GUI_INPUT_TOUCH_EVENT_QUEUE_SIZE);

    // View ports
    ViewHandleArray_init(gui->views);

    // Display and buffer
    gui->display = display_jd9853_qspi_init();
    size_t canvas_buf_size = canvas_get_required_buffer_size(JD9853_WIDTH, JD9853_HEIGHT);
    gui->render_canvas = canvas_alloc(JD9853_WIDTH, JD9853_HEIGHT);
    FURI_LOG_I(TAG, "Canvas %ux%u: %zu bytes (framebuffer %zu bytes)", JD9853_WIDTH, JD9853_HEIGHT, canvas_buf_size, (size_t)JD9853_WIDTH * JD9853_HEIGHT);

    // Clay initialization
    Clay_SetMaxElementCount(CLAY_MAX_ELEMENT_COUNT);
    Clay_SetMaxMeasureTextCacheWordCount(CLAY_MAX_MEASURE_TEXT_CACHE_WORDS);
    uint64_t clay_memory = Clay_MinMemorySize();
    FURI_LOG_I(
        TAG,
        "Clay arena: %llu bytes (~%llu KiB), elements=%u, text_cache=%u words",
        clay_memory,
        clay_memory / 1024,
        CLAY_MAX_ELEMENT_COUNT,
        CLAY_MAX_MEASURE_TEXT_CACHE_WORDS);
    Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(clay_memory, malloc(clay_memory));
    Clay_Initialize(arena, (Clay_Dimensions){JD9853_WIDTH, JD9853_HEIGHT}, (Clay_ErrorHandler){gui_handle_clay_errors, gui});
    Clay_SetMeasureTextFunction(clay_render_measure_text, NULL);

    //Gui callback initialization
    gui->callback_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    GuiCallbackArray_init(gui->gui_callbacks_pair);

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

size_t gui_get_width(Gui* gui) {
    furi_check(gui);
    return canvas_get_width(gui->render_canvas);
}

size_t gui_get_height(Gui* gui) {
    furi_check(gui);
    return canvas_get_height(gui->render_canvas);
}

void gui_display_frame(Gui* gui, const uint8_t* data) {
    furi_check(gui);
    furi_check(data);

    gui_lock(gui);

    size_t width = canvas_get_width(gui->render_canvas);
    size_t height = canvas_get_height(gui->render_canvas);
    size_t size = width * height;

    /* Fast path: the incoming frame already matches the canvas/display format
     * (8-bit grayscale, full screen). Blit it into the canvas and push it to
     * the display without running Clay. Framebuffer callbacks (e.g. RPC screen
     * streaming) still get every frame because they read the same canvas. */
    memcpy(canvas_get_data(gui->render_canvas), data, size);
    display_jd9853_qspi_write_buffer(gui->display, canvas_get_data(gui->render_canvas), size);

    furi_check(furi_mutex_acquire(gui->callback_mutex, FuriWaitForever) == FuriStatusOk);
    for
        M_EACH(p, gui->gui_callbacks_pair, GuiCallbackArray_t) {
            p->callback(canvas_get_data(gui->render_canvas), width, height, p->context);
        }
    furi_mutex_release(gui->callback_mutex);

    gui_unlock(gui);
}

void gui_add_framebuffer_callback(Gui* gui, GuiFramebufferCallback callback, void* context) {
    furi_check(gui);

    const GuiCallbackPair pair = {.callback = callback, .context = context};

    furi_check(furi_mutex_acquire(gui->callback_mutex, FuriWaitForever) == FuriStatusOk);

    furi_check(!GuiCallbackArray_count(gui->gui_callbacks_pair, pair));
    GuiCallbackArray_push_back(gui->gui_callbacks_pair, pair);

    furi_mutex_release(gui->callback_mutex);
}

void gui_remove_framebuffer_callback(Gui* gui, GuiFramebufferCallback callback, void* context) {
    furi_check(gui);

    const GuiCallbackPair pair = {.callback = callback, .context = context};

    furi_check(furi_mutex_acquire(gui->callback_mutex, FuriWaitForever) == FuriStatusOk);

    furi_check(GuiCallbackArray_count(gui->gui_callbacks_pair, pair) == 1);
    GuiCallbackArray_remove_val(gui->gui_callbacks_pair, pair);

    furi_mutex_release(gui->callback_mutex);
}
