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
    Desktop* desktop = context;
    View* view = scene_get_view(scene);
    furi_check(view);

    scene_set_data(scene, desktop);

    PopupMenu* menu = popup_menu_alloc(view);
    popup_menu_set_callback(menu, power_menu_item_selected, desktop);
    popup_menu_add_item(menu, "Start Flipper OS", PowerMenuItemStartCpu);
    popup_menu_add_item(menu, "Power Off", PowerMenuItemPowerOff);
}

const SceneCallbacks scene_power_menu_callbacks = {
    .on_alloc = power_menu_on_alloc,
    .on_enter = NULL,
    .on_exit = NULL,
    .on_event = NULL,
};
