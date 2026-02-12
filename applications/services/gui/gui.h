#pragma once
#include <input/input.h>
#include <input_touch/input_touch.h>
#include "view.h"

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

void gui_add_view(Gui* gui, View* view, GuiViewPriority priority);

void gui_remove_view(Gui* gui, View* view);

#ifdef __cplusplus
}
#endif
