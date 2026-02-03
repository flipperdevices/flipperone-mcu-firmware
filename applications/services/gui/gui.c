#include "gui_i.h"
#include "view_port_i.h"

#define TAG "GuiSrv"

#define GUI_INPUT_EVENT_QUEUE_SIZE       32
#define GUI_INPUT_TOUCH_EVENT_QUEUE_SIZE 32

#define GUI_EVENT_FLAG_REDRAW (1U << 0)

static ViewPort* gui_view_port_find_enabled(ViewPortArray_t array) {
    // Iterating backward
    ViewPortArray_it_t it;
    ViewPortArray_it_last(it, array);
    while(!ViewPortArray_end_p(it)) {
        ViewPort* view_port = *ViewPortArray_ref(it);
        if(view_port_is_enabled(view_port)) {
            return view_port;
        }
        ViewPortArray_previous(it);
    }
    return NULL;
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

    view_port_layout(gui_view_port_find_enabled(gui->layers[GuiLayerFullscreen]));

    Clay_RenderCommandArray renderCommands = Clay_EndLayout();

    render_clear_buffer(0xFF);
    render_do_render(&renderCommands);

    view_port_post_layout(gui_view_port_find_enabled(gui->layers[GuiLayerFullscreen]));

    size_t width = render_get_buffer_width(gui->render_buffer);
    size_t height = render_get_buffer_height(gui->render_buffer);
    display_jd9853_qspi_write_buffer(gui->display, render_get_buffer_data(gui->render_buffer), width * height);

    gui_unlock(gui);
}

static void gui_input_touch(Gui* gui, InputTouchEvent* input_event) {
    furi_assert(gui);
    furi_assert(input_event);

    // Check input complementarity
    if(input_event->type == InputTouchTypeEnd) {
        gui->onging_touch_input = false;
    } else if(input_event->type == InputTouchTypeStart) {
        gui->onging_touch_input = true;
    } else if(!gui->onging_touch_input) {
        FURI_LOG_D(TAG, "non-complementary touch, discarding type: %d", input_event->type);
        return;
    }

    gui_lock(gui);

    do {
        ViewPort* view_port = NULL;

        view_port = gui_view_port_find_enabled(gui->layers[GuiLayerFullscreen]);

        if(gui->onging_touch_input && input_event->type == InputTouchTypeStart) {
            gui->ongoing_input_view_port = view_port;
        }

        if(view_port && view_port == gui->ongoing_input_view_port) {
            view_port_input_touch(view_port, input_event);
        } else if(gui->ongoing_input_view_port && input_event->type == InputTouchTypeEnd) {
            FURI_LOG_D(
                TAG,
                "ViewPort changed while touch %p -> %p. Sending touch type: %d to previous view port",
                gui->ongoing_input_view_port,
                view_port,
                input_event->type);
            view_port_input_touch(gui->ongoing_input_view_port, input_event);
        } else {
            FURI_LOG_D(TAG, "ViewPort changed while key press %p -> %p. Discarding touch type: %d", gui->ongoing_input_view_port, view_port, input_event->type);
        }
    } while(false);

    gui_unlock(gui);
}

static void gui_input(Gui* gui, InputEvent* input_event) {
    furi_assert(gui);
    furi_assert(input_event);

    // Check input complementarity
    uint8_t key_bit = input_event->key;
    if(input_event->type == InputTypeRelease) {
        gui->ongoing_input &= ~key_bit;
    } else if(input_event->type == InputTypePress) {
        gui->ongoing_input |= key_bit;
    } else if(!(gui->ongoing_input & key_bit)) {
        FURI_LOG_D(
            TAG,
            "non-complementary input, discarding key: %s type: %s, sequence: %p",
            input_get_key_name(input_event->key),
            input_get_type_name(input_event->type),
            (void*)input_event->sequence);
        return;
    }

    gui_lock(gui);

    do {
        ViewPort* view_port = NULL;

        view_port = gui_view_port_find_enabled(gui->layers[GuiLayerFullscreen]);

        if(!(gui->ongoing_input & ~key_bit) && input_event->type == InputTypePress) {
            gui->ongoing_input_view_port = view_port;
        }

        if(view_port && view_port == gui->ongoing_input_view_port) {
            view_port_input(view_port, input_event);
        } else if(gui->ongoing_input_view_port && input_event->type == InputTypeRelease) {
            FURI_LOG_D(
                TAG,
                "ViewPort changed while key press %p -> %p. Sending key: %s, type: %s, sequence: %p to previous view port",
                gui->ongoing_input_view_port,
                view_port,
                input_get_key_name(input_event->key),
                input_get_type_name(input_event->type),
                (void*)input_event->sequence);
            view_port_input(gui->ongoing_input_view_port, input_event);
        } else {
            FURI_LOG_D(
                TAG,
                "ViewPort changed while key press %p -> %p. Discarding key: %s, type: %s, sequence: %p",
                gui->ongoing_input_view_port,
                view_port,
                input_get_key_name(input_event->key),
                input_get_type_name(input_event->type),
                (void*)input_event->sequence);
        }
    } while(false);

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

void gui_add_view_port(Gui* gui, ViewPort* view_port, GuiLayer layer) {
    furi_check(gui);
    furi_check(view_port);
    furi_check(layer < GuiLayerMAX);

    gui_lock(gui);
    // Verify that view port is not yet added
    ViewPortArray_it_t it;
    for(size_t i = 0; i < GuiLayerMAX; i++) {
        ViewPortArray_it(it, gui->layers[i]);
        while(!ViewPortArray_end_p(it)) {
            furi_assert(*ViewPortArray_ref(it) != view_port);
            ViewPortArray_next(it);
        }
    }
    // Add view port and link with gui
    ViewPortArray_push_back(gui->layers[layer], view_port);
    view_port_gui_set(view_port, gui);
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
    for(size_t i = 0; i < GuiLayerMAX; i++) {
        ViewPortArray_it(it, gui->layers[i]);
        while(!ViewPortArray_end_p(it)) {
            if(*ViewPortArray_ref(it) == view_port) {
                ViewPortArray_remove(gui->layers[i], it);
            } else {
                ViewPortArray_next(it);
            }
        }
    }
    if(gui->ongoing_input_view_port == view_port) {
        gui->ongoing_input_view_port = NULL;
    }
    gui_unlock(gui);

    // Request redraw
    gui_update(gui);
}

void gui_view_port_send_to_front(Gui* gui, ViewPort* view_port) {
    furi_check(gui);
    furi_check(view_port);

    gui_lock(gui);
    // Remove
    GuiLayer layer = GuiLayerMAX;
    ViewPortArray_it_t it;
    for(size_t i = 0; i < GuiLayerMAX; i++) {
        ViewPortArray_it(it, gui->layers[i]);
        while(!ViewPortArray_end_p(it)) {
            if(*ViewPortArray_ref(it) == view_port) {
                ViewPortArray_remove(gui->layers[i], it);
                furi_check(layer == GuiLayerMAX);
                layer = i;
            } else {
                ViewPortArray_next(it);
            }
        }
    }
    furi_check(layer != GuiLayerMAX);
    // Return to the top
    ViewPortArray_push_back(gui->layers[layer], view_port);
    gui_unlock(gui);

    // Request redraw
    gui_update(gui);
}

void gui_view_port_send_to_back(Gui* gui, ViewPort* view_port) {
    furi_assert(gui);
    furi_assert(view_port);

    gui_lock(gui);
    // Remove
    GuiLayer layer = GuiLayerMAX;
    ViewPortArray_it_t it;
    for(size_t i = 0; i < GuiLayerMAX; i++) {
        ViewPortArray_it(it, gui->layers[i]);
        while(!ViewPortArray_end_p(it)) {
            if(*ViewPortArray_ref(it) == view_port) {
                ViewPortArray_remove(gui->layers[i], it);
                furi_assert(layer == GuiLayerMAX);
                layer = i;
            } else {
                ViewPortArray_next(it);
            }
        }
    }
    furi_assert(layer != GuiLayerMAX);
    // Return to the top
    ViewPortArray_push_at(gui->layers[layer], 0, view_port);
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

    // Layers
    for(size_t i = 0; i < GuiLayerMAX; i++) {
        ViewPortArray_init(gui->layers[i]);
    }

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
