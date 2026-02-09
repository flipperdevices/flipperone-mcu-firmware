#pragma once
#include <input/input.h>
#include <input_touch/input_touch.h>
#include "view_port.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Gui Gui;

#define RECORD_GUI "Gui"

typedef enum {
    GuiViewPriorityDesktop = 0,
    GuiViewPriorityApplication = 50000,
    GuiViewPriorityMenu = 100000,
} GuiViewPriority;

void gui_add_view_port(Gui* gui, ViewPort* view_port, GuiViewPriority priority);

void gui_remove_view_port(Gui* gui, ViewPort* view_port);

void gui_view_port_send_to_front(Gui* gui, ViewPort* view_port);

void gui_view_port_send_to_back(Gui* gui, ViewPort* view_port);

#ifdef __cplusplus
}
#endif
