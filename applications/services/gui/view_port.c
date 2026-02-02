#include "view_port_i.h"

#define TAG "ViewPort"

struct ViewPort {
    Gui* gui;
    FuriMutex* mutex;
    bool is_enabled;

    ViewPortLayoutCallback callback_layout;
    ViewPortPostLayoutCallback callback_post_layout;
    ViewPortInputCallback callback_input;
    ViewPortInputTouchCallback callback_input_touch;
    void* callback_layout_context;
    void* callback_input_context;
};

ViewPort* view_port_alloc(void) {
    ViewPort* view_port = malloc(sizeof(ViewPort));
    view_port->is_enabled = true;
    view_port->mutex = furi_mutex_alloc(FuriMutexTypeRecursive);
    return view_port;
}

void view_port_free(ViewPort* view_port) {
    furi_check(view_port);
    furi_check(furi_mutex_acquire(view_port->mutex, FuriWaitForever) == FuriStatusOk);
    furi_check(view_port->gui == NULL);
    furi_check(furi_mutex_release(view_port->mutex) == FuriStatusOk);
    furi_mutex_free(view_port->mutex);
    free(view_port);
}

void view_port_set_input_callbacks(
    ViewPort* view_port,
    ViewPortInputCallback input_callback,
    ViewPortInputTouchCallback input_touch_callback,
    void* input_context) {
    furi_check(view_port);
    furi_check(furi_mutex_acquire(view_port->mutex, FuriWaitForever) == FuriStatusOk);
    view_port->callback_input = input_callback;
    view_port->callback_input_touch = input_touch_callback;
    view_port->callback_input_context = input_context;
    furi_check(furi_mutex_release(view_port->mutex) == FuriStatusOk);
}

void view_port_set_layout_callbacks(
    ViewPort* view_port,
    ViewPortLayoutCallback layout_callback,
    ViewPortPostLayoutCallback post_layout_callback,
    void* layout_context) {
    furi_check(view_port);
    furi_check(furi_mutex_acquire(view_port->mutex, FuriWaitForever) == FuriStatusOk);
    view_port->callback_layout = layout_callback;
    view_port->callback_post_layout = post_layout_callback;
    view_port->callback_layout_context = layout_context;
    furi_check(furi_mutex_release(view_port->mutex) == FuriStatusOk);
}

bool view_port_is_enabled(const ViewPort* view_port) {
    furi_check(view_port);
    furi_check(furi_mutex_acquire(view_port->mutex, FuriWaitForever) == FuriStatusOk);
    bool is_enabled = view_port->is_enabled;
    furi_check(furi_mutex_release(view_port->mutex) == FuriStatusOk);
    return is_enabled;
}

void view_port_layout(ViewPort* view_port) {
    furi_check(view_port);

    // We are not going to lockup system, but will notify you instead
    // Make sure that you don't call viewport methods inside of another mutex, especially one that is used in draw call
    if(furi_mutex_acquire(view_port->mutex, 2) != FuriStatusOk) {
        FURI_LOG_W(TAG, "ViewPort lockup: see %s:%d", __FILE__, __LINE__ - 3);
    }

    furi_check(view_port->gui);

    if(view_port->callback_layout) {
        view_port->callback_layout(view_port->callback_layout_context);
    }

    furi_mutex_release(view_port->mutex);
}

void view_port_post_layout(ViewPort* view_port) {
    furi_check(view_port);

    // We are not going to lockup system, but will notify you instead
    // Make sure that you don't call viewport methods inside of another mutex, especially one that is used in draw call
    if(furi_mutex_acquire(view_port->mutex, 2) != FuriStatusOk) {
        FURI_LOG_W(TAG, "ViewPort lockup: see %s:%d", __FILE__, __LINE__ - 3);
    }

    furi_check(view_port->gui);

    if(view_port->callback_post_layout) {
        view_port->callback_post_layout(view_port->callback_layout_context);
    }

    furi_mutex_release(view_port->mutex);
}

void view_port_input(ViewPort* view_port, InputEvent* event) {
    furi_assert(view_port);
    furi_assert(event);

    furi_check(furi_mutex_acquire(view_port->mutex, FuriWaitForever) == FuriStatusOk);
    furi_check(view_port->gui);
    if(view_port->callback_input) {
        view_port->callback_input(event, view_port->callback_input_context);
    }
    furi_check(furi_mutex_release(view_port->mutex) == FuriStatusOk);
}

void view_port_input_touch(ViewPort* view_port, InputTouchEvent* event) {
    furi_assert(view_port);
    furi_assert(event);

    furi_check(furi_mutex_acquire(view_port->mutex, FuriWaitForever) == FuriStatusOk);
    furi_check(view_port->gui);
    if(view_port->callback_input_touch) {
        view_port->callback_input_touch(event, view_port->callback_input_context);
    }
    furi_check(furi_mutex_release(view_port->mutex) == FuriStatusOk);
}

void view_port_gui_set(ViewPort* view_port, Gui* gui) {
    furi_check(view_port);
    furi_check(furi_mutex_acquire(view_port->mutex, FuriWaitForever) == FuriStatusOk);
    view_port->gui = gui;
    furi_check(furi_mutex_release(view_port->mutex) == FuriStatusOk);
}
