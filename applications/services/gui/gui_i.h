#pragma once

#include <furi.h>
#include <m-array.h>
#include "gui.h"
#include "view_port.h"
#include <gui_test/clay.h>
#include <gui_test/clay_render.h>
#include <drivers/display/display_jd9853_qspi.h>
#include <drivers/display/display_jd9853_reg.h>

#define GUI_THREAD_FLAG_DRAW        (1 << 0)
#define GUI_THREAD_FLAG_INPUT       (1 << 1)
#define GUI_THREAD_FLAG_INPUT_TOUCH (1 << 2)
#define GUI_THREAD_FLAG_ALL         (GUI_THREAD_FLAG_DRAW | GUI_THREAD_FLAG_INPUT | GUI_THREAD_FLAG_INPUT_TOUCH)

ARRAY_DEF(ViewPortArray, ViewPort*, M_PTR_OPLIST);

/** Gui structure */
struct Gui {
    // Thread and lock
    FuriThreadId thread_id;
    FuriMutex* mutex;

    // Layers and Canvas
    ViewPortArray_t layers[GuiLayerMAX];
    RenderBuffer* render_buffer;
    DisplayJd9853QSPI* display;
    void* arena_memory;

    // Input
    FuriPubSub* input_events;
    FuriMessageQueue* input_queue;

    // Touch Input
    FuriPubSub* input_touch_events;
    FuriMessageQueue* input_touch_queue;

    ViewPort* ongoing_input_view_port;
    uint32_t ongoing_input;
    bool onging_touch_input;
};
