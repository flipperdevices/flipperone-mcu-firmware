#include "../desktop_i.h"
#include "../scene.h"
#include <gui/modules/menu.h>
#include <led/led.h>

#define TAG "PowerSettings"

typedef enum {
    PowerSettingsItemBattery = 0,
    PowerSettingsItemAutoOff,
} PowerSettingsItem;

static const struct {
    const char* text;
    uint32_t value;
} auto_off_time_values[] = {
    {"1 min", 1 * 60},
    {"3 min", 3 * 60},
    {"5 min", 5 * 60},
    {"10 min", 10 * 60},
    {"15 min", 15 * 60},
    {"30 min", 30 * 60},
    {"1 hour", 60 * 60},
    {"Never", 0},
};

typedef struct {
    Desktop* desktop;
    Menu* menu;
    MenuItem* auto_off_item;
} PowerSettingsData;

static void power_settings_menu_callback(MenuItem* item, size_t item_id, void* context) {
    furi_check(context);
    Scene* scene = context;
    PowerSettingsData* scene_data = scene_get_data(scene);

    if(item == NULL) {
        FURI_LOG_I(TAG, "Exit");
        desktop_send_scene_event(scene_data->desktop, DesktopSceneEventTypeEnterSettingsMenu, scene);
    } else if(item_id == PowerSettingsItemBattery) {
    }
}

static void power_settings_auto_off_callback(MenuItem* item, size_t selector_index, void* context) {
    furi_check(context);
    Scene* scene = context;
    PowerSettingsData* scene_data = scene_get_data(scene);

    uint32_t time = auto_off_time_values[selector_index].value;
    menu_item_selector_set_value(scene_data->menu, scene_data->auto_off_item, selector_index, auto_off_time_values[selector_index].text);
}

static void power_settings_on_enter(Scene* scene, void* context) {
    View* view = scene_get_view(scene);
    PowerSettingsData* scene_data = malloc(sizeof(PowerSettingsData));
    scene_data->desktop = context;
    scene_set_data(scene, scene_data);

    scene_data->menu = menu_alloc(view);
    menu_set_callback(scene_data->menu, power_settings_menu_callback, scene);
    menu_set_title(scene_data->menu, "> Settings > Power Management");

    menu_add_item(scene_data->menu, "Battery", PowerSettingsItemBattery, MenuItemSubTypeNone);

    scene_data->auto_off_item = menu_add_item(scene_data->menu, "Power OFF when inactive", PowerSettingsItemAutoOff, MenuItemSubTypeSelector);
    menu_item_selector_configure(scene_data->menu, scene_data->auto_off_item, COUNT_OF(auto_off_time_values), power_settings_auto_off_callback);
    menu_item_selector_set_value(scene_data->menu, scene_data->auto_off_item, 0, auto_off_time_values[0].text);
}

static void power_settings_on_exit(Scene* scene, void* context) {
    UNUSED(context);
    PowerSettingsData* scene_data = scene_get_data(scene);
    menu_free(scene_data->menu);
    free(scene_data);
    scene_set_data(scene, NULL);
}

const SceneCallbacks scene_power_settings_callbacks = {
    .on_alloc = NULL,
    .on_enter = power_settings_on_enter,
    .on_exit = power_settings_on_exit,
    .on_event = NULL,
};
