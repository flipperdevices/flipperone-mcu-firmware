#include <assets.h>
#include <gui/gui.h>
#include <gui/modules/popup_menu.h>
#include <gui/clay_helper.h>
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
        desktop_send_scene_event(desktop, DesktopSceneEventTypeTogglePowerMenu, NULL);
    } else if(item_id == PowerMenuItemStartCpu) {
        desktop_start_cpu(false);
        desktop_send_scene_event(desktop, DesktopSceneEventTypeTogglePowerMenu, NULL);
    } else if(item_id == PowerMenuItemPowerOff) {
        desktop_power_off();
    }
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
}

const SceneCallbacks scene_power_menu_callbacks = {
    .on_alloc = power_menu_on_alloc,
    .on_enter = power_menu_on_enter,
    .on_exit = NULL,
    .on_event = NULL,
};
