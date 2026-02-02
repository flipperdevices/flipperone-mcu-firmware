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
 */
typedef void (*ViewPortInputCallback)(InputEvent* event, void* context);

/** ViewPort Input Touch callback
 * @warning    called from GUI thread
 */
typedef void (*ViewPortInputTouchCallback)(InputTouchEvent* event, void* context);

/** ViewPort post layout callback
 * @warning    called from GUI thread
 */
typedef void (*ViewPortPostLayoutCallback)(void* context);

ViewPort* view_port_alloc(void);

void view_port_free(ViewPort* view_port);

void view_port_set_input_callbacks(
    ViewPort* view_port,
    ViewPortInputCallback input_callback,
    ViewPortInputTouchCallback input_touch_callback,
    void* input_context);

void view_port_set_layout_callbacks(
    ViewPort* view_port,
    ViewPortLayoutCallback layout_callback,
    ViewPortPostLayoutCallback post_layout_callback,
    void* layout_context);

bool view_port_is_enabled(const ViewPort* view_port);

void view_port_layout(ViewPort* view_port);

void view_port_post_layout(ViewPort* view_port);

void view_port_input(ViewPort* view_port, InputEvent* event);

void view_port_input_touch(ViewPort* view_port, InputTouchEvent* event);

#ifdef __cplusplus
}
#endif
