#pragma once

#include <furi.h>
#include <gui/view.h>

typedef struct Menu Menu;
typedef struct MenuItem MenuItem;

/* Secondary element types */
typedef enum {
    MenuItemSubTypeNone = 0,
    MenuItemSubTypeLabel,
    MenuItemSubTypeSelector,
} MenuItemSubType;

typedef void (*MenuItemCallback)(MenuItem* item, size_t item_id, void* context);

typedef void (*MenuItemSelectorCallback)(MenuItem* item, size_t selector_index, void* context);

Menu* menu_alloc(View* view);

void menu_free(Menu* menu);

void menu_set_title(Menu* menu, const char* title);

void menu_set_position(Menu* menu, size_t item_id);

void menu_set_callback(Menu* menu, MenuItemCallback callback, void* context);

MenuItem* menu_add_item(Menu* menu, const char* label, size_t id, MenuItemSubType type);

// TODO: menu_remove_item for dynamic menus, needs dict instead of array

void menu_item_sublabel_set(Menu* menu, MenuItem* item, const char* text);

void menu_item_selector_configure(Menu* menu, MenuItem* item, size_t max_count, MenuItemSelectorCallback callback);

void menu_item_selector_set_value(Menu* menu, MenuItem* item, size_t index, const char* text);

size_t menu_item_selector_get_value(Menu* menu, MenuItem* item);
