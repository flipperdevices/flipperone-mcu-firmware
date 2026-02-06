#pragma once
#include <input/input.h>
#include <input_touch/input_touch.h>
#include "view_port.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Gui layers */
typedef enum {
    GuiLayerFullscreen, /**< Fullscreen layer, no status bar */
    GuiLayerMAX /**< Don't use or move, special value */
} GuiLayer;

typedef struct Gui Gui;

#define RECORD_GUI "Gui"

void gui_add_view_port(Gui* gui, ViewPort* view_port, GuiLayer layer);

void gui_remove_view_port(Gui* gui, ViewPort* view_port);

void gui_view_port_send_to_front(Gui* gui, ViewPort* view_port);

void gui_view_port_send_to_back(Gui* gui, ViewPort* view_port);

#ifdef __cplusplus
}
#endif