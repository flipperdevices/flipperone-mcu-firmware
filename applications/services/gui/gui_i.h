#pragma once

#include <furi.h>
#include <m-array.h>
#include "gui.h"
#include "view_port.h"
#include "clay.h"
#include "clay_render.h"
#include <drivers/display/display_jd9853_qspi.h>
#include <drivers/display/display_jd9853_reg.h>

ARRAY_DEF(ViewPortArray, ViewPort*, M_PTR_OPLIST);

/** Gui structure */
struct Gui {
    // Global gui mutex
    FuriMutex* mutex;

    // Layers and Canvas
    ViewPortArray_t layers[GuiLayerMAX];
    RenderBuffer* render_buffer;
    DisplayJd9853QSPI* display;

    // Event handling
    FuriEventLoop* event_loop;
    FuriEventFlag* redraw_flag;
    FuriMessageQueue* input_queue;
    FuriMessageQueue* input_touch_queue;

    // Ongoing thingy
    ViewPort* ongoing_input_view_port;
    uint32_t ongoing_input;
    bool onging_touch_input;
};

void gui_update(Gui* gui);

void gui_lock(Gui* gui);

void gui_unlock(Gui* gui);
