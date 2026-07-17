#pragma once

#include <furi.h>
#include <gui/view.h>

#define POPUP_MENU_EXIT_ID SIZE_MAX // Special ID for exit/back action, not a real item ID

typedef struct PopupMenu PopupMenu;
typedef struct PopupMenuItem PopupMenuItem;

typedef void (*PopupMenuCallback)(size_t item_id, void* context);

PopupMenu* popup_menu_alloc(View* view);

void popup_menu_free(PopupMenu* menu);

void popup_menu_set_title(PopupMenu* menu, const char* title);

void popup_menu_set_position(PopupMenu* menu, size_t item_id);

void popup_menu_set_visible(PopupMenu* menu, bool visible);

void popup_menu_set_callback(PopupMenu* menu, PopupMenuCallback callback, void* context);

void popup_menu_add_item(PopupMenu* menu, const char* label, size_t index);

// TODO: popup_menu_remove_item for dynamic menus, needs dict instead of array
