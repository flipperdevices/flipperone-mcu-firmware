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

void gui_lock(Gui* gui);

void gui_unlock(Gui* gui);

void gui_add_view_port(Gui* gui, ViewPort* view_port, GuiLayer layer);

void gui_update(Gui* gui);

#ifdef __cplusplus
}
#endif