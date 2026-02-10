#include "gui.h"
#include "view.h"

/** Set view's GUI instance
 * @param view 
 * @param gui 
 */
void view_gui_set(View* view, Gui* gui);

/** Call layout callback for the view
 * @param view 
 */
void view_layout(View* view);

/** Call post layout callback for the view. Full layout is ready at this point, so we can manipulate with it.
 * @param view 
 */
void view_post_layout(View* view);

/** Call input callback for the view
 * @param view 
 * @param event 
 * @return true if the input event was consumed and should not be propagated to other view ports
 */
bool view_input(View* view, InputEvent* event);

/** Call input touch callback for the view
 * @param view 
 * @param event 
 * @return true if the input touch event was consumed and should not be propagated to other view ports
 */
bool view_input_touch(View* view, InputTouchEvent* event);
