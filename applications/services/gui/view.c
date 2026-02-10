#include "gui_i.h"
#include "view_i.h"

#define TAG "View"

typedef struct {
    ViewLayoutCallback callback;
} LayoutCallback;

typedef struct {
    ViewLayoutCallback callback;
} PostLayoutCallback;

typedef struct {
    ViewInputCallback callback;
    void* context;
} InputCallback;

typedef struct {
    ViewInputTouchCallback callback;
    void* context;
} InputTouchCallback;

typedef struct {
    FuriMutex* mutex;
    uint8_t data[];
} ViewModelLocking;

struct View {
    Gui* gui;
    FuriMutex* mutex;
    bool is_enabled;
    bool is_transparent;

    LayoutCallback layout;
    PostLayoutCallback post_layout;
    InputCallback input;
    InputTouchCallback input_touch;

    ViewModelType model_type;
    void* model;
    void* context;
};

View* view_alloc(void) {
    View* view = malloc(sizeof(View));
    view->is_enabled = true;
    view->mutex = furi_mutex_alloc(FuriMutexTypeRecursive);
    view->model_type = ViewModelTypeNone;
    view->model = NULL;
    view->context = NULL;
    return view;
}

void view_free(View* view) {
    furi_check(view);
    furi_check(furi_mutex_acquire(view->mutex, FuriWaitForever) == FuriStatusOk);
    furi_check(view->gui == NULL);
    furi_check(furi_mutex_release(view->mutex) == FuriStatusOk);
    furi_mutex_free(view->mutex);
    view_free_model(view);
    free(view);
}

void view_allocate_model(View* view, ViewModelType type, size_t size) {
    furi_check(view);
    furi_check(size > 0);
    furi_check(view->model_type == ViewModelTypeNone);
    furi_check(view->model == NULL);
    view->model_type = type;
    if(view->model_type == ViewModelTypeLockFree) {
        view->model = malloc(size);
    } else if(view->model_type == ViewModelTypeLocking) {
        ViewModelLocking* model = malloc(sizeof(ViewModelLocking) + size);
        model->mutex = furi_mutex_alloc(FuriMutexTypeRecursive);
        view->model = model;
    } else {
        furi_crash();
    }
}

void view_free_model(View* view) {
    furi_check(view);
    if(view->model_type == ViewModelTypeNone) {
        return;
    } else if(view->model_type == ViewModelTypeLocking) {
        ViewModelLocking* model = view->model;
        furi_mutex_free(model->mutex);
    }
    free(view->model);
    view->model = NULL;
    view->model_type = ViewModelTypeNone;
}

static void view_unlock_model(View* view) {
    furi_check(view);
    if(view->model_type == ViewModelTypeLocking) {
        ViewModelLocking* model = (ViewModelLocking*)(view->model);
        furi_check(furi_mutex_release(model->mutex) == FuriStatusOk);
    }
}

static void view_update(View* view) {
    furi_check(view);

    // We are not going to lockup system, but will notify you instead
    // Make sure that you don't call view methods inside of another mutex, especially one that is used in draw call
    if(furi_mutex_acquire(view->mutex, 2) != FuriStatusOk) {
        FURI_LOG_W(TAG, "View lockup: see %s:%d", __FILE__, __LINE__ - 3);
    }

    if(view->gui && view->is_enabled) gui_update(view->gui);
    furi_mutex_release(view->mutex);
}

void view_set_input_callback(View* view, ViewInputCallback input_callback, void* input_context) {
    furi_check(view);
    furi_check(furi_mutex_acquire(view->mutex, FuriWaitForever) == FuriStatusOk);
    view->input.callback = input_callback;
    view->input.context = input_context;
    furi_check(furi_mutex_release(view->mutex) == FuriStatusOk);
}

void view_set_input_touch_callback(View* view, ViewInputTouchCallback input_touch_callback, void* input_touch_context) {
    furi_check(view);
    furi_check(furi_mutex_acquire(view->mutex, FuriWaitForever) == FuriStatusOk);
    view->input_touch.callback = input_touch_callback;
    view->input_touch.context = input_touch_context;
    furi_check(furi_mutex_release(view->mutex) == FuriStatusOk);
}

void view_set_layout_callback(View* view, ViewLayoutCallback layout_callback) {
    furi_check(view);
    furi_check(furi_mutex_acquire(view->mutex, FuriWaitForever) == FuriStatusOk);
    view->layout.callback = layout_callback;
    furi_check(furi_mutex_release(view->mutex) == FuriStatusOk);
}

void view_set_post_layout_callback(View* view, ViewLayoutCallback post_layout_callback) {
    furi_check(view);
    furi_check(furi_mutex_acquire(view->mutex, FuriWaitForever) == FuriStatusOk);
    view->post_layout.callback = post_layout_callback;
    furi_check(furi_mutex_release(view->mutex) == FuriStatusOk);
}

bool view_is_enabled(const View* view) {
    furi_check(view);
    furi_check(furi_mutex_acquire(view->mutex, FuriWaitForever) == FuriStatusOk);
    bool is_enabled = view->is_enabled;
    furi_check(furi_mutex_release(view->mutex) == FuriStatusOk);
    return is_enabled;
}

bool view_is_transparent(const View* view) {
    furi_check(view);
    furi_check(furi_mutex_acquire(view->mutex, FuriWaitForever) == FuriStatusOk);
    bool is_transparent = view->is_transparent;
    furi_check(furi_mutex_release(view->mutex) == FuriStatusOk);
    return is_transparent;
}

void view_set_enabled(View* view, bool enabled) {
    furi_check(view);
    furi_check(furi_mutex_acquire(view->mutex, FuriWaitForever) == FuriStatusOk);
    view->is_enabled = enabled;
    furi_check(furi_mutex_release(view->mutex) == FuriStatusOk);
}

void view_set_transparent(View* view, bool transparent) {
    furi_check(view);
    furi_check(furi_mutex_acquire(view->mutex, FuriWaitForever) == FuriStatusOk);
    view->is_transparent = transparent;
    furi_check(furi_mutex_release(view->mutex) == FuriStatusOk);
}

void view_layout(View* view) {
    furi_check(view);

    // We are not going to lockup system, but will notify you instead
    // Make sure that you don't call view methods inside of another mutex, especially one that is used in draw call
    if(furi_mutex_acquire(view->mutex, 16) != FuriStatusOk) {
        FURI_LOG_W(TAG, "View lockup: see %s:%d", __FILE__, __LINE__ - 3);
    }

    furi_check(view->gui);

    if(view->layout.callback) {
        void* data = view_get_model(view);
        if(view->layout.callback(data)) {
            // force next frame
            view_update(view);
        }
        view_unlock_model(view);
    }

    furi_mutex_release(view->mutex);
}

void view_post_layout(View* view) {
    furi_check(view);

    // We are not going to lockup system, but will notify you instead
    // Make sure that you don't call view methods inside of another mutex, especially one that is used in draw call
    if(furi_mutex_acquire(view->mutex, 16) != FuriStatusOk) {
        FURI_LOG_W(TAG, "View lockup: see %s:%d", __FILE__, __LINE__ - 3);
    }

    furi_check(view->gui);

    if(view->post_layout.callback) {
        void* data = view_get_model(view);
        if(view->post_layout.callback(data)) {
            // force next frame
            view_update(view);
        }
        view_unlock_model(view);
    }

    furi_mutex_release(view->mutex);
}

bool view_input(View* view, InputEvent* event) {
    furi_assert(view);
    furi_assert(event);
    bool consumed = false;

    furi_check(furi_mutex_acquire(view->mutex, FuriWaitForever) == FuriStatusOk);
    furi_check(view->gui);
    if(view->input.callback) {
        consumed = view->input.callback(event, view->input.context);
    }
    furi_check(furi_mutex_release(view->mutex) == FuriStatusOk);
    return consumed;
}

bool view_input_touch(View* view, InputTouchEvent* event) {
    furi_assert(view);
    furi_assert(event);
    bool consumed = false;

    furi_check(furi_mutex_acquire(view->mutex, FuriWaitForever) == FuriStatusOk);
    furi_check(view->gui);
    if(view->input_touch.callback) {
        consumed = view->input_touch.callback(event, view->input_touch.context);
    }
    furi_check(furi_mutex_release(view->mutex) == FuriStatusOk);
    return consumed;
}

void view_gui_set(View* view, Gui* gui) {
    furi_check(view);
    furi_check(furi_mutex_acquire(view->mutex, FuriWaitForever) == FuriStatusOk);
    view->gui = gui;
    furi_check(furi_mutex_release(view->mutex) == FuriStatusOk);
}

void* view_get_model(View* view) {
    furi_check(view);
    if(view->model_type == ViewModelTypeLocking) {
        ViewModelLocking* model = (ViewModelLocking*)(view->model);
        furi_check(furi_mutex_acquire(model->mutex, FuriWaitForever) == FuriStatusOk);
        return model->data;
    }
    return view->model;
}

void view_commit_model(View* view, bool update) {
    furi_check(view);
    view_unlock_model(view);
    if(update) {
        view_update(view);
    }
}
