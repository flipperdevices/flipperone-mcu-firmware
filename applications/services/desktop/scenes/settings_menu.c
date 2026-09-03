#include "../desktop_i.h"
#include "../scene.h"
#include <gui/modules/menu.h>
#include <led/led.h>

#define TAG "SettingsMenu"

typedef enum {
    SettingsMenuItemDisplay = 0,
    SettingsMenuItemPower,
    SettingsMenuItemSelfCheck,
    SettingsMenuItemMaskrom,
    SettingsMenuItemTesting,
    SettingsMenuItemInfo,
} SettingsMenuItem;

typedef struct {
    Desktop* desktop;
    Menu* menu;
    MenuItem* brightness_item;
    FuriStateSub* brightness_state_sub;
} SettingsMenuData;

static void settings_menu_item_callback(MenuItem* item, size_t item_id, void* context) {
    furi_check(context);
    Scene* scene = context;
    SettingsMenuData* scene_data = scene_get_data(scene);

    if(item == NULL) {
        FURI_LOG_I(TAG, "Exit");
        desktop_send_scene_event(scene_data->desktop, DesktopSceneEventTypeReturnToDesktop, scene);
    } else if(item_id == SettingsMenuItemDisplay) {
        desktop_send_scene_event(scene_data->desktop, DesktopSceneEventTypeEnterDisplaySettings, scene);
    } else if(item_id == SettingsMenuItemPower) {
        desktop_send_scene_event(scene_data->desktop, DesktopSceneEventTypeEnterPowerSettings, scene);
    } else if(item_id == SettingsMenuItemSelfCheck) {
        desktop_start_app_by_id("self_check");
    } else if(item_id == SettingsMenuItemMaskrom) {
        desktop_start_cpu(true);
    } else if(item_id == SettingsMenuItemTesting) {
        desktop_send_scene_event(scene_data->desktop, DesktopSceneEventTypeEnterTestingMenu, scene);
    } else if(item_id == SettingsMenuItemInfo) {
        // desktop_send_scene_event(scene_data->desktop, DesktopSceneEventTypeOpenDebugMenu, NULL);
    }
}

static void brightness_state_callback(const void* item, void* context) {
    uint8_t* brightness = (uint8_t*)item;
    furi_check(context);
    SettingsMenuData* scene_data = context;

    FuriString* backlight_text = furi_string_alloc();
    if(*brightness == 0) {
        furi_string_printf(backlight_text, "Backlight OFF");
    } else {
        uint8_t brightness_pct = lroundf((float)*brightness * 100.f / 255.f);
        furi_string_printf(backlight_text, "Backlight %u%%", brightness_pct);
    }
    menu_item_sublabel_set(scene_data->menu, scene_data->brightness_item, furi_string_get_cstr(backlight_text));
    furi_string_free(backlight_text);
}

static void settings_menu_on_enter(Scene* scene, void* context) {
    View* view = scene_get_view(scene);
    SettingsMenuData* scene_data = malloc(sizeof(SettingsMenuData));
    scene_data->desktop = context;
    scene_set_data(scene, scene_data);

    scene_data->menu = menu_alloc(view);
    menu_set_callback(scene_data->menu, settings_menu_item_callback, scene);
    menu_set_title(scene_data->menu, "> Settings");

    scene_data->brightness_item = menu_add_item(scene_data->menu, "Display", SettingsMenuItemDisplay, MenuItemSubTypeLabel);
    Led* led = furi_record_open(RECORD_LEDS);
    scene_data->brightness_state_sub = furi_state_subscribe(led_get_brightness_state(led, LedGroupDisplayBacklight), brightness_state_callback, scene_data);
    furi_record_close(RECORD_LEDS);

    menu_add_item(scene_data->menu, "Power Management", SettingsMenuItemPower, MenuItemSubTypeNone);
    menu_add_item(scene_data->menu, "Self Check", SettingsMenuItemSelfCheck, MenuItemSubTypeNone);
    menu_add_item(scene_data->menu, "Maskrom", SettingsMenuItemMaskrom, MenuItemSubTypeNone);
    menu_add_item(scene_data->menu, "Testing", SettingsMenuItemTesting, MenuItemSubTypeNone);
    menu_add_item(scene_data->menu, "Info", SettingsMenuItemInfo, MenuItemSubTypeNone);
}

static void settings_menu_on_exit(Scene* scene, void* context) {
    UNUSED(context);
    SettingsMenuData* scene_data = scene_get_data(scene);
    furi_state_unsubscribe(scene_data->brightness_state_sub);
    menu_free(scene_data->menu);
    free(scene_data);
    scene_set_data(scene, NULL);
}

const SceneCallbacks scene_settings_menu_callbacks = {
    .on_alloc = NULL,
    .on_enter = settings_menu_on_enter,
    .on_exit = settings_menu_on_exit,
    .on_event = NULL,
};
