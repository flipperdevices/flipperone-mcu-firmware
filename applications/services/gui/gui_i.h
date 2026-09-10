#pragma once

#include "gui.h"

void gui_update(Gui* gui);

void gui_lock(Gui* gui);

void gui_unlock(Gui* gui);

/* Internal: whether the registered popup menu overlay is currently visible. */
bool popup_menu_is_visible(PopupMenu* menu);
