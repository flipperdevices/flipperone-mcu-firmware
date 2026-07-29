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
    GuiViewPriorityStatusBar = 0,
    GuiViewPriorityDesktop = 1,

    GuiViewPriorityApplication = 50000,
    GuiViewPriorityPowerMenu = 100000,
} GuiViewPriority;

void gui_add_view(Gui* gui, View* view, GuiViewPriority priority);

void gui_remove_view(Gui* gui, View* view);

void gui_add_unhandled_input_callback(Gui* gui, ViewInputCallback callback, void* context);
void gui_add_unhandled_touch_input_callback(Gui* gui, ViewInputTouchCallback callback, void* context);

typedef void (*GuiFramebufferCallback)(const uint8_t* data, size_t width, size_t height, void* context);

typedef struct {
    GuiFramebufferCallback callback;
    void* context;
} GuiCallbackPair;

void gui_add_framebuffer_callback(Gui* gui, GuiFramebufferCallback callback, void* context);
void gui_remove_framebuffer_callback(Gui* gui, GuiFramebufferCallback callback, void* context);

size_t gui_get_width(Gui* gui);
size_t gui_get_height(Gui* gui);

#ifdef __cplusplus
}
#endif
