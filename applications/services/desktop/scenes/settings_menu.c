#include <assets.h>
#include <gui/gui.h>
#include <gui/clay_helper.h>
#include <gui/modules/menu.h>
#include "../scene.h"
#include "../elements.h"
#include "../desktop_i.h"
#include "scene_events.h"

#define TAG "SettingsMenu"

static const char* test_options[] = {
    "Option 1",
    "Option 2",
    "Option 3",
};

static void settings_menu_item_callback(MenuItem* item, size_t item_id, void* context) {
    if(item == NULL) {
        FURI_LOG_I(TAG, "Exit");
        // scene_exit(scene, desktop);
    } else {
        size_t selector_index = 0;
        if(item_id == 3) {
            selector_index = menu_item_selector_get_value((Menu*)context, item);
        }
        FURI_LOG_I(TAG, "Item selected: %u %u", item_id, selector_index);
    }
}

static void settings_selector_callback(MenuItem* item, size_t selector_index, void* context) {
    FURI_LOG_I(TAG, "Selector changed: %u", selector_index);
    menu_item_selector_set_value((Menu*)context, item, selector_index, test_options[selector_index]);
}

static void settings_menu_on_alloc(Scene* scene, void* context) {
    View* view = scene_get_view(scene);
    scene_set_data(scene, context);

    Menu* menu = menu_alloc(view);
    menu_set_callback(menu, settings_menu_item_callback, menu);
    menu_set_title(menu, "> Settings");

    MenuItem* item = menu_add_item(menu, "Display", 0, MenuItemSubTypeLabel);
    menu_item_sublabel_set(menu, item, "Backlight 10%");

    menu_add_item(menu, "Power Management", 1, MenuItemSubTypeNone);
    menu_add_item(menu, "Self Check", 2, MenuItemSubTypeNone);

    item = menu_add_item(menu, "Line 3", 3, MenuItemSubTypeSelector);
    menu_item_selector_configure(menu, item, COUNT_OF(test_options), settings_selector_callback);
    menu_item_selector_set_value(menu, item, 0, test_options[0]);

    FuriString* line_name = furi_string_alloc();
    for(size_t i = 0; i < 20; i++) {
        furi_string_printf(line_name, "Line %u", i + 4);
        menu_add_item(menu, furi_string_get_cstr(line_name), i + 4, MenuItemSubTypeSelector);
    }
    furi_string_free(line_name);
}

const SceneCallbacks scene_settings_menu_callbacks = {
    .on_alloc = settings_menu_on_alloc,
    .on_enter = NULL,
    .on_exit = NULL,
    .on_event = NULL,
};
