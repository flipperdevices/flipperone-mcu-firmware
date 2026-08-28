#include <assets.h>
#include <gui/gui.h>
#include <gui/modules/popup_menu.h>
#include <gui/clay_helper.h>
#include <power/power.h>
#include "../scene.h"
#include "../desktop_i.h"

#define TAG "DesktopPowerMenu"

typedef enum {
    PowerMenuItemStartCpu = 0,
    PowerMenuItemPowerOff,
} PowerMenuItem;

typedef struct {
    Desktop* desktop;
    PopupMenu* menu;
} PowerMenuData;

static void power_menu_item_selected(size_t item_id, void* context) {
    Desktop* desktop = context;
    if(item_id == POPUP_MENU_EXIT_ID) {
        FURI_LOG_I(TAG, "Exit");
        desktop_send_scene_event(desktop, DesktopSceneEventTypeTogglePowerMenu, desktop);
    } else if(item_id == PowerMenuItemStartCpu) {
        desktop_start_cpu(false);
        desktop_send_scene_event(desktop, DesktopSceneEventTypeTogglePowerMenu, desktop);
    } else if(item_id == PowerMenuItemPowerOff) {
        desktop_power_off();
    }
}

static void power_menu_update_consumption(Scene* scene) {
    PowerMenuData* scene_data = scene_get_data(scene);
    furi_check(scene_data);

    Power* power = furi_record_open(RECORD_POWER);

    PowerDevice devices = 0;
    power_is_device_initialized(power, &devices);

    if(devices & PowerDeviceIna219) {
        float_t voltage = power_ina219_get_voltage_v(power);
        float_t current = power_ina219_get_current_a(power);
        float_t consumption = power_ina219_get_power_w(power);

        popup_menu_set_status(scene_data->menu, "%.1fV %.2fA %.2fW", (double)voltage, (double)current, (double)consumption);
    } else {
        popup_menu_set_status(scene_data->menu, NULL);
    }

    furi_record_close(RECORD_POWER);
}

static void power_menu_on_alloc(Scene* scene, void* context) {
    PowerMenuData* scene_data = malloc(sizeof(PowerMenuData));
    scene_data->desktop = context;
    scene_set_data(scene, scene_data);

    View* view = scene_get_view(scene);
    furi_check(view);

    scene_data->menu = popup_menu_alloc(view);
    popup_menu_set_callback(scene_data->menu, power_menu_item_selected, scene_data->desktop);
    popup_menu_add_item(scene_data->menu, "Start Flipper OS", PowerMenuItemStartCpu);
    popup_menu_add_item(scene_data->menu, "Power Off", PowerMenuItemPowerOff);
}

static void power_menu_on_enter(Scene* scene, void* app) {
    PowerMenuData* scene_data = scene_get_data(scene);
    furi_check(scene_data);

    popup_menu_set_position(scene_data->menu, PowerMenuItemStartCpu);
    popup_menu_set_visible(scene_data->menu, true);

    power_menu_update_consumption(scene);
}

static void power_menu_on_exit(Scene* scene, void* context) {
    UNUSED(context);
    PowerMenuData* scene_data = scene_get_data(scene);
    furi_check(scene_data);

    popup_menu_set_visible(scene_data->menu, false);
}

static bool power_menu_on_event(Scene* scene, uint32_t event, void* data) {
    UNUSED(data);
    PowerMenuData* scene_data = scene_get_data(scene);
    furi_check(scene_data);

    bool consumed = false;

    if(event == DesktopSceneEventTypePowerUpdate) {
        if(popup_menu_is_visible(scene_data->menu)) {
            power_menu_update_consumption(scene);
        }
        consumed = true;
    }

    return consumed;
}

const SceneCallbacks scene_power_menu_callbacks = {
    .on_alloc = power_menu_on_alloc,
    .on_enter = power_menu_on_enter,
    .on_exit = power_menu_on_exit,
    .on_event = power_menu_on_event,
};
