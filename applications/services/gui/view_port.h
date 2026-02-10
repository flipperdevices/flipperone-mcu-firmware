#pragma once
#include <input/input.h>
#include <input_touch/input_touch.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ViewPort ViewPort;

/** ViewPort Layout callback
 * @warning    called from GUI thread
 * @param      model   pointer to view model data
 * @return true if view needs to be redrawn after layout, false otherwise
 */
typedef bool (*ViewPortLayoutCallback)(void* model);

/** ViewPort Input callback
 * @warning    called from GUI thread
 * @return true if the input event was consumed and should not be propagated to other view ports
 */
typedef bool (*ViewPortInputCallback)(InputEvent* event, void* context);

/** ViewPort Input Touch callback
 * @warning    called from GUI thread
 * @return true if the input touch event was consumed and should not be propagated to other view ports
 */
typedef bool (*ViewPortInputTouchCallback)(InputTouchEvent* event, void* context);

/** View model types */
typedef enum {
    /** Model is not allocated */
    ViewPortModelTypeNone,
    /** Model consist of atomic types and/or partial update is not critical for rendering.
     * Lock free.
     */
    ViewPortModelTypeLockFree,
    /** Model access is guarded with mutex.
     * Locking gui thread.
     */
    ViewPortModelTypeLocking,
} ViewPortModelType;

ViewPort* view_port_alloc(void);

void view_port_free(ViewPort* view_port);

/** Allocate view model.
 * @param view_port  ViewPort instance
 * @param      type  View Model Type
 * @param      size  size
 */
void view_port_allocate_model(ViewPort* view_port, ViewPortModelType type, size_t size);

/** Free view model data memory.
 * @param view_port  ViewPort instance
 */
void view_port_free_model(ViewPort* view_port);

void view_port_set_input_callback(ViewPort* view_port, ViewPortInputCallback input_callback, void* input_context);

void view_port_set_input_touch_callback(ViewPort* view_port, ViewPortInputTouchCallback input_touch_callback, void* input_touch_context);

void view_port_set_layout_callback(ViewPort* view_port, ViewPortLayoutCallback layout_callback);

void view_port_set_post_layout_callback(ViewPort* view_port, ViewPortLayoutCallback post_layout_callback);

bool view_port_is_enabled(const ViewPort* view_port);

bool view_port_is_transparent(const ViewPort* view_port);

void view_port_set_enabled(ViewPort* view_port, bool enabled);

void view_port_set_transparent(ViewPort* view_port, bool transparent);

void view_port_update(ViewPort* view_port);

/** Get view model data
 * @param      view_port  ViewPort instance
 * @return     pointer to model data
 * @warning    Don't forget to commit model changes
 */
void* view_port_get_model(ViewPort* view_port);

/** Commit view model
 * @param   view_port    ViewPort instance
 * @param      update  true if you want to emit view update, false otherwise
 */
void view_port_commit_model(ViewPort* view_port, bool update);

/** With clause for view model
 *
 * @param      view_port      ViewPort instance pointer
 * @param      type           View model type
 * @param      code           Code block that will be executed between model lock and unlock
 * @param      update         Bool flag, if true, view will be updated after code block. Can be variable, so code block can decide if update is needed.
 *
 */
#define with_view_model(view_port, type, code, update) \
    {                                                  \
        type = view_port_get_model(view_port);         \
        {code};                                        \
        view_port_commit_model(view_port, update);     \
    }

#ifdef __cplusplus
}
#endif
