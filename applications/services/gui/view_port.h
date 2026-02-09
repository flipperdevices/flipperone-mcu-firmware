#pragma once
#include <input/input.h>
#include <input_touch/input_touch.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ViewPort ViewPort;

/** ViewPort Layout callback
 * @warning    called from GUI thread
 */
typedef void (*ViewPortLayoutCallback)(void* context);

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

ViewPort* view_port_alloc(void);

void view_port_free(ViewPort* view_port);

void view_port_set_input_callback(ViewPort* view_port, ViewPortInputCallback input_callback, void* input_context);

void view_port_set_input_touch_callback(ViewPort* view_port, ViewPortInputTouchCallback input_touch_callback, void* input_touch_context);

void view_port_set_layout_callback(ViewPort* view_port, ViewPortLayoutCallback layout_callback, void* layout_context);

void view_port_set_post_layout_callback(ViewPort* view_port, ViewPortLayoutCallback post_layout_callback, void* post_layout_context);

bool view_port_is_enabled(const ViewPort* view_port);

bool view_port_is_transparent(const ViewPort* view_port);

void view_port_set_enabled(ViewPort* view_port, bool enabled);

void view_port_update(ViewPort* view_port);

/**
 * @brief Glue function to put input events into a message queue, to use with view_port_set_input_callback
 * @warning Expects context to be of type FuriMessageQueue pointer
 * @param event 
 * @param context 
 * @return always true (the input is always consumed)
 */
bool view_port_input_queue_glue(InputEvent* event, void* context);

/**
 * @brief Glue function to put touch input events into a message queue to use with view_port_set_input_touch_callback
 * @warning Expects context to be of type FuriMessageQueue pointer
 * @param event 
 * @param context 
 * @return always true (the input is always consumed)
 */
bool view_port_input_touch_queue_glue(InputTouchEvent* event, void* context);

#ifdef __cplusplus
}
#endif
