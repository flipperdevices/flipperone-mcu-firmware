#include "gui.h"
#include "view_port.h"

void view_port_gui_set(ViewPort* view_port, Gui* gui);

void view_port_layout(ViewPort* view_port);

void view_port_post_layout(ViewPort* view_port);

void view_port_input(ViewPort* view_port, InputEvent* event);

void view_port_input_touch(ViewPort* view_port, InputTouchEvent* event);
