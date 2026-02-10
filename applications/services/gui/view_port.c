#include "gui_i.h"
#include "view_port_i.h"

#define TAG "ViewPort"

typedef struct {
    ViewPortLayoutCallback callback;
} LayoutCallback;

typedef struct {
    ViewPortLayoutCallback callback;
} PostLayoutCallback;

typedef struct {
    ViewPortInputCallback callback;
    void* context;
} InputCallback;

typedef struct {
    ViewPortInputTouchCallback callback;
    void* context;
} InputTouchCallback;

typedef struct {
    FuriMutex* mutex;
    uint8_t data[];
} ViewPortModelLocking;

struct ViewPort {
    Gui* gui;
    FuriMutex* mutex;
    bool is_enabled;
    bool is_transparent;

    LayoutCallback layout;
    PostLayoutCallback post_layout;
    InputCallback input;
    InputTouchCallback input_touch;

    ViewPortModelType model_type;
    void* model;
    void* context;
};

ViewPort* view_port_alloc(void) {
    ViewPort* view_port = malloc(sizeof(ViewPort));
    view_port->is_enabled = true;
    view_port->mutex = furi_mutex_alloc(FuriMutexTypeRecursive);
    view_port->model_type = ViewPortModelTypeNone;
    view_port->model = NULL;
    view_port->context = NULL;
    return view_port;
}

void view_port_free(ViewPort* view_port) {
    furi_check(view_port);
    furi_check(furi_mutex_acquire(view_port->mutex, FuriWaitForever) == FuriStatusOk);
    furi_check(view_port->gui == NULL);
    furi_check(furi_mutex_release(view_port->mutex) == FuriStatusOk);
    furi_mutex_free(view_port->mutex);
    view_port_free_model(view_port);
    free(view_port);
}

void view_port_allocate_model(ViewPort* view_port, ViewPortModelType type, size_t size) {
    furi_check(view_port);
    furi_check(size > 0);
    furi_check(view_port->model_type == ViewPortModelTypeNone);
    furi_check(view_port->model == NULL);
    view_port->model_type = type;
    if(view_port->model_type == ViewPortModelTypeLockFree) {
        view_port->model = malloc(size);
    } else if(view_port->model_type == ViewPortModelTypeLocking) {
        ViewPortModelLocking* model = malloc(sizeof(ViewPortModelLocking) + size);
        model->mutex = furi_mutex_alloc(FuriMutexTypeRecursive);
        view_port->model = model;
    } else {
        furi_crash();
    }
}

void view_port_free_model(ViewPort* view_port) {
    furi_check(view_port);
    if(view_port->model_type == ViewPortModelTypeNone) {
        return;
    } else if(view_port->model_type == ViewPortModelTypeLocking) {
        ViewPortModelLocking* model = view_port->model;
        furi_mutex_free(model->mutex);
    }
    free(view_port->model);
    view_port->model = NULL;
    view_port->model_type = ViewPortModelTypeNone;
}

static void view_unlock_model(ViewPort* view_port) {
    furi_check(view_port);
    if(view_port->model_type == ViewPortModelTypeLocking) {
        ViewPortModelLocking* model = (ViewPortModelLocking*)(view_port->model);
        furi_check(furi_mutex_release(model->mutex) == FuriStatusOk);
    }
}

void view_port_set_input_callback(ViewPort* view_port, ViewPortInputCallback input_callback, void* input_context) {
    furi_check(view_port);
    furi_check(furi_mutex_acquire(view_port->mutex, FuriWaitForever) == FuriStatusOk);
    view_port->input.callback = input_callback;
    view_port->input.context = input_context;
    furi_check(furi_mutex_release(view_port->mutex) == FuriStatusOk);
}

void view_port_set_input_touch_callback(ViewPort* view_port, ViewPortInputTouchCallback input_touch_callback, void* input_touch_context) {
    furi_check(view_port);
    furi_check(furi_mutex_acquire(view_port->mutex, FuriWaitForever) == FuriStatusOk);
    view_port->input_touch.callback = input_touch_callback;
    view_port->input_touch.context = input_touch_context;
    furi_check(furi_mutex_release(view_port->mutex) == FuriStatusOk);
}

void view_port_set_layout_callback(ViewPort* view_port, ViewPortLayoutCallback layout_callback) {
    furi_check(view_port);
    furi_check(furi_mutex_acquire(view_port->mutex, FuriWaitForever) == FuriStatusOk);
    view_port->layout.callback = layout_callback;
    furi_check(furi_mutex_release(view_port->mutex) == FuriStatusOk);
}

void view_port_set_post_layout_callback(ViewPort* view_port, ViewPortLayoutCallback post_layout_callback) {
    furi_check(view_port);
    furi_check(furi_mutex_acquire(view_port->mutex, FuriWaitForever) == FuriStatusOk);
    view_port->post_layout.callback = post_layout_callback;
    furi_check(furi_mutex_release(view_port->mutex) == FuriStatusOk);
}

bool view_port_is_enabled(const ViewPort* view_port) {
    furi_check(view_port);
    furi_check(furi_mutex_acquire(view_port->mutex, FuriWaitForever) == FuriStatusOk);
    bool is_enabled = view_port->is_enabled;
    furi_check(furi_mutex_release(view_port->mutex) == FuriStatusOk);
    return is_enabled;
}

bool view_port_is_transparent(const ViewPort* view_port) {
    furi_check(view_port);
    furi_check(furi_mutex_acquire(view_port->mutex, FuriWaitForever) == FuriStatusOk);
    bool is_transparent = view_port->is_transparent;
    furi_check(furi_mutex_release(view_port->mutex) == FuriStatusOk);
    return is_transparent;
}

void view_port_set_enabled(ViewPort* view_port, bool enabled) {
    furi_check(view_port);
    furi_check(furi_mutex_acquire(view_port->mutex, FuriWaitForever) == FuriStatusOk);
    view_port->is_enabled = enabled;
    furi_check(furi_mutex_release(view_port->mutex) == FuriStatusOk);
}

void view_port_set_transparent(ViewPort* view_port, bool transparent) {
    furi_check(view_port);
    furi_check(furi_mutex_acquire(view_port->mutex, FuriWaitForever) == FuriStatusOk);
    view_port->is_transparent = transparent;
    furi_check(furi_mutex_release(view_port->mutex) == FuriStatusOk);
}

void view_port_layout(ViewPort* view_port) {
    furi_check(view_port);

    // We are not going to lockup system, but will notify you instead
    // Make sure that you don't call viewport methods inside of another mutex, especially one that is used in draw call
    if(furi_mutex_acquire(view_port->mutex, 16) != FuriStatusOk) {
        FURI_LOG_W(TAG, "ViewPort lockup: see %s:%d", __FILE__, __LINE__ - 3);
    }

    furi_check(view_port->gui);

    if(view_port->layout.callback) {
        void* data = view_port_get_model(view_port);
        if(view_port->layout.callback(data)) {
            // force next frame
            view_port_update(view_port);
        }
        view_unlock_model(view_port);
    }

    furi_mutex_release(view_port->mutex);
}

void view_port_post_layout(ViewPort* view_port) {
    furi_check(view_port);

    // We are not going to lockup system, but will notify you instead
    // Make sure that you don't call viewport methods inside of another mutex, especially one that is used in draw call
    if(furi_mutex_acquire(view_port->mutex, 16) != FuriStatusOk) {
        FURI_LOG_W(TAG, "ViewPort lockup: see %s:%d", __FILE__, __LINE__ - 3);
    }

    furi_check(view_port->gui);

    if(view_port->post_layout.callback) {
        void* data = view_port_get_model(view_port);
        if(view_port->post_layout.callback(data)) {
            // force next frame
            view_port_update(view_port);
        }
        view_unlock_model(view_port);
    }

    furi_mutex_release(view_port->mutex);
}

bool view_port_input(ViewPort* view_port, InputEvent* event) {
    furi_assert(view_port);
    furi_assert(event);
    bool consumed = false;

    furi_check(furi_mutex_acquire(view_port->mutex, FuriWaitForever) == FuriStatusOk);
    furi_check(view_port->gui);
    if(view_port->input.callback) {
        consumed = view_port->input.callback(event, view_port->input.context);
    }
    furi_check(furi_mutex_release(view_port->mutex) == FuriStatusOk);
    return consumed;
}

bool view_port_input_touch(ViewPort* view_port, InputTouchEvent* event) {
    furi_assert(view_port);
    furi_assert(event);
    bool consumed = false;

    furi_check(furi_mutex_acquire(view_port->mutex, FuriWaitForever) == FuriStatusOk);
    furi_check(view_port->gui);
    if(view_port->input_touch.callback) {
        consumed = view_port->input_touch.callback(event, view_port->input_touch.context);
    }
    furi_check(furi_mutex_release(view_port->mutex) == FuriStatusOk);
    return consumed;
}

void view_port_gui_set(ViewPort* view_port, Gui* gui) {
    furi_check(view_port);
    furi_check(furi_mutex_acquire(view_port->mutex, FuriWaitForever) == FuriStatusOk);
    view_port->gui = gui;
    furi_check(furi_mutex_release(view_port->mutex) == FuriStatusOk);
}

void view_port_update(ViewPort* view_port) {
    furi_check(view_port);

    // We are not going to lockup system, but will notify you instead
    // Make sure that you don't call viewport methods inside of another mutex, especially one that is used in draw call
    if(furi_mutex_acquire(view_port->mutex, 2) != FuriStatusOk) {
        FURI_LOG_W(TAG, "ViewPort lockup: see %s:%d", __FILE__, __LINE__ - 3);
    }

    if(view_port->gui && view_port->is_enabled) gui_update(view_port->gui);
    furi_mutex_release(view_port->mutex);
}

void* view_port_get_model(ViewPort* view_port) {
    furi_check(view_port);
    if(view_port->model_type == ViewPortModelTypeLocking) {
        ViewPortModelLocking* model = (ViewPortModelLocking*)(view_port->model);
        furi_check(furi_mutex_acquire(model->mutex, FuriWaitForever) == FuriStatusOk);
        return model->data;
    }
    return view_port->model;
}

void view_port_commit_model(ViewPort* view_port, bool update) {
    furi_check(view_port);
    view_unlock_model(view_port);
    if(update) {
        view_port_update(view_port);
    }
}
