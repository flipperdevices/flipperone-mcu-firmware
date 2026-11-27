#pragma once

#include <furi.h>
#include <gui/view.h>
#include <gui/image.h>
#include <input/input.h>

#define POPUP_MENU_EXIT_ID SIZE_MAX // Special ID for exit/back action, not a real item ID

typedef struct PopupMenu PopupMenu;
typedef struct PopupMenuItem PopupMenuItem;

typedef void (*PopupMenuCallback)(size_t item_id, void* context);

/** Look of the key the menu draws: it matches the screen the menu opens over */
typedef enum {
    PopupMenuSoftkeyStyleClassic,
    PopupMenuSoftkeyStyleHomeTab,
} PopupMenuSoftkeyStyle;

/** Where the labels sit in their lines */
typedef enum {
    PopupMenuAlignCenter, // Power Menu style
    PopupMenuAlignLeft, // Help style
} PopupMenuAlign;

PopupMenu* popup_menu_alloc(View* view);

void popup_menu_free(PopupMenu* menu);

/** The key that opens and closes the menu and the softkey drawn active over it,
 * at the index and with the label the screen under the menu gives it. Default:
 * the Power key, classic softkey 2 "Power". The label must outlive the menu. */
void popup_menu_set_softkey(PopupMenu* menu, PopupMenuSoftkeyStyle style, size_t index, const char* label, InputKey key);

void popup_menu_set_title(PopupMenu* menu, const char* title);

/** The 14x14 icon before the title, none by default. It must outlive the menu. */
void popup_menu_set_icon(PopupMenu* menu, const Image* icon);

/** Default PopupMenuAlignCenter */
void popup_menu_set_align(PopupMenu* menu, PopupMenuAlign align);

void popup_menu_set_position(PopupMenu* menu, size_t item_id);

void popup_menu_set_visible(PopupMenu* menu, bool visible);

void popup_menu_set_callback(PopupMenu* menu, PopupMenuCallback callback, void* context);

void popup_menu_add_item(PopupMenu* menu, const char* label, size_t id);

// TODO: popup_menu_remove_item for dynamic menus, needs dict instead of array
