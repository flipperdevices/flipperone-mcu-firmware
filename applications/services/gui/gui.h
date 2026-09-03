#pragma once
#include <input/input.h>
#include <input_touch/input_touch.h>
#include "view.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Gui Gui;

typedef struct PopupMenu PopupMenu;

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

/**
 * Feed a full-screen frame to the GUI for display.
 *
 * The frame must be in the exact canvas/display format (8-bit grayscale,
 * width*height bytes, full screen). The GUI decides how to present it: if no
 * overlay (e.g. the power menu) is shown on top, it is blitted straight to the
 * display, bypassing Clay; otherwise it is composited via Clay under the
 * overlay.
 *
 * The pointer is only used transiently (copied out during the next redraw), so
 * the caller must keep it valid until then.
 */
void gui_push_frame(Gui* gui, const uint8_t* data);

/**
 * Drop the pending full-screen frame (if any) and request a redraw.
 *
 * Call this when the frame source goes away (e.g. the owning app exits or
 * the device feeding frames is reset) so the next redraw falls back to the
 * normal Clay compositing instead of blitting a stale frame.
 */
void gui_clear_frame(Gui* gui);

/**
 * Register the popup menu used as an overlay on top of pushed frames.
 *
 * The GUI checks its visibility to decide between the fast direct-blit path
 * and the Clay-composited path (menu drawn over the frame). Pass NULL to
 * clear the reference (e.g. when the owning application exits).
 */
void gui_set_menu(Gui* gui, PopupMenu* menu);

#ifdef __cplusplus
}
#endif
