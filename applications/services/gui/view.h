#pragma once
#include <input/input.h>
#include <input_touch/input_touch.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct View View;

/** View Layout callback
 * @warning    called from GUI thread
 * @param      model   pointer to view model data
 * @return true if view needs to be redrawn after layout, false otherwise
 */
typedef bool (*ViewLayoutCallback)(void* model);

/** View Input callback
 * @warning    called from GUI thread
 * @return true if the input event was consumed and should not be propagated to other view ports
 */
typedef bool (*ViewInputCallback)(InputEvent* event, void* context);

/** View Input Touch callback
 * @warning    called from GUI thread
 * @return true if the input touch event was consumed and should not be propagated to other view ports
 */
typedef bool (*ViewInputTouchCallback)(InputTouchEvent* event, void* context);

/** View model types */
typedef enum {
    /** Model is not allocated */
    ViewModelTypeNone,
    /** Model consist of atomic types and/or partial update is not critical for rendering.
     * Lock free.
     */
    ViewModelTypeLockFree,
    /** Model access is guarded with mutex.
     * Locking gui thread.
     */
    ViewModelTypeLocking,
} ViewModelType;

/** Allocate view instance.
 * @return pointer to allocated view instance
 */
View* view_alloc(void);

/** Free view instance.
 * @param view  View instance pointer
 */
void view_free(View* view);

/** Allocate view model.
 * @param view  View instance
 * @param type  View Model Type
 * @param size  size of model data in bytes
 */
void view_allocate_model(View* view, ViewModelType type, size_t size);

/** Free view model data memory. Usually not needed, because view model memory will be freed when view is freed.
 * @param view  View instance
 */
void view_free_model(View* view);

/** Set view input callback. Called from GUI thread.
 * @param view              View instance
 * @param input_callback    Input callback function pointer
 * @param input_context     Input callback context pointer
 */
void view_set_input_callback(View* view, ViewInputCallback input_callback, void* input_context);

/** Set view input touch callback. Called from GUI thread.
 * @param view                       View instance
 * @param input_touch_callback       Input touch callback function pointer
 * @param input_touch_context        Input touch callback context pointer
 */
void view_set_input_touch_callback(View* view, ViewInputTouchCallback input_touch_callback, void* input_touch_context);

/** Set view layout callback. Called from GUI thread.
 * @param view              View instance
 * @param layout_callback   Layout callback function pointer
 */
void view_set_layout_callback(View* view, ViewLayoutCallback layout_callback);

/** Set view post layout callback. Called from GUI thread after layout is done.
 * Useful for doing some operations after layout is done, like updating the layout result.
 * @param view                  View instance
 * @param post_layout_callback  Post layout callback function pointer
 */
void view_set_post_layout_callback(View* view, ViewLayoutCallback post_layout_callback);

/** Check if view is enabled 
 * @param view  View instance
 * @return true if view is enabled, false otherwise
*/
bool view_is_enabled(const View* view);

/** Check if view is transparent
 * @param view  View instance
 * @return true if view is transparent, false if view is opaque
 */
bool view_is_transparent(const View* view);

/** Set view enabled or disabled. Disabled view will not receive input events and will not be rendered.
 * @param view      View instance
 * @param enabled   true to enable view, false to disable view
 */
void view_set_enabled(View* view, bool enabled);

/** Set view transparent or opaque.
 * @param view          View instance
 * @param transparent   true to make view transparent, false to make view opaque
 */
void view_set_transparent(View* view, bool transparent);

/** Get view model data
 * @param      view  View instance
 * @return     pointer to model data
 * @warning    Don't forget to commit model changes
 */
void* view_get_model(View* view);

/** Commit view model
 * @param   view    View instance
 * @param   update  true if you want to emit view update, false otherwise
 */
void view_commit_model(View* view, bool update);

/** With clause for view model
 *
 * @param   view    View instance pointer
 * @param   type    View model type
 * @param   code    Code block that will be executed between model lock and unlock
 * @param   update  Bool flag, if true, view will be updated after code block. Can be variable, so code block can decide if update is needed.
 *
 */
#define with_view_model(view, type, code, update) \
    {                                             \
        type = view_get_model(view);              \
        {code};                                   \
        view_commit_model(view, update);          \
    }

#ifdef __cplusplus
}
#endif
