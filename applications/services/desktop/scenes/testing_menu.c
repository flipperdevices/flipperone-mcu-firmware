#include "../desktop_i.h"
#include "../scene.h"
#include <gui/modules/menu.h>
#include <led/led.h>

#define TAG "TestingMenu"

typedef enum {
    TestingMenuItemLeds = 0,
    TestingMenuItemKeypad,
    TestingMenuItemTouchpad,
    TestingMenuItemHaptic,
} TestingMenuItem;

typedef struct {
    Desktop* desktop;
    Menu* menu;
} TestingMenuData;

extern int32_t keypad_test_app(void* p);
extern int32_t touchpad_test_app(void* p);
extern int32_t haptic_test_app(void* p);

static const FlipperInternalApplication test_keypad_app = {
    .app = keypad_test_app,
    .name = "Keypad Test",
    .appid = "keypad_test",
    .stack_size = 2048,
    .flags = FlipperInternalApplicationFlagDefault,
};

static const FlipperInternalApplication test_touchpad_app = {
    .app = touchpad_test_app,
    .name = "Touchpad Test",
    .appid = "touchpad_test",
    .stack_size = 2048,
    .flags = FlipperInternalApplicationFlagDefault,
};

static const FlipperInternalApplication test_haptic_app = {
    .app = haptic_test_app,
    .name = "Haptic Test",
    .appid = "haptic_test",
    .stack_size = 2048,
    .flags = FlipperInternalApplicationFlagDefault,
};

static void testing_menu_callback(MenuItem* item, size_t item_id, void* context) {
    furi_check(context);
    Scene* scene = context;
    TestingMenuData* scene_data = scene_get_data(scene);

    if(item == NULL) {
        desktop_send_scene_event(scene_data->desktop, DesktopSceneEventTypeEnterSettingsMenu, scene);
    } else if(item_id == TestingMenuItemLeds) {
        desktop_send_scene_event(scene_data->desktop, DesktopSceneEventTypeEnterLedsMenu, scene);
    } else if(item_id == TestingMenuItemKeypad) {
        desktop_start_app(&test_keypad_app);
    } else if(item_id == TestingMenuItemTouchpad) {
        desktop_start_app(&test_touchpad_app);
    } else if(item_id == TestingMenuItemHaptic) {
        desktop_start_app(&test_haptic_app);
    }
}

static void testing_menu_on_enter(Scene* scene, void* context) {
    View* view = scene_get_view(scene);
    TestingMenuData* scene_data = malloc(sizeof(TestingMenuData));
    scene_data->desktop = context;
    scene_set_data(scene, scene_data);

    scene_data->menu = menu_alloc(view);
    menu_set_callback(scene_data->menu, testing_menu_callback, scene);
    menu_set_title(scene_data->menu, "> Settings > Testing");

    menu_add_item(scene_data->menu, "LEDs", TestingMenuItemLeds, MenuItemSubTypeNone);
    menu_add_item(scene_data->menu, "Keypad", TestingMenuItemKeypad, MenuItemSubTypeNone);
    menu_add_item(scene_data->menu, "Touchpad", TestingMenuItemTouchpad, MenuItemSubTypeNone);
    menu_add_item(scene_data->menu, "Haptic", TestingMenuItemHaptic, MenuItemSubTypeNone);
}

static void testing_menu_on_exit(Scene* scene, void* context) {
    UNUSED(context);
    TestingMenuData* scene_data = scene_get_data(scene);
    menu_free(scene_data->menu);
    free(scene_data);
    scene_set_data(scene, NULL);
}

const SceneCallbacks scene_testing_menu_callbacks = {
    .on_alloc = NULL,
    .on_enter = testing_menu_on_enter,
    .on_exit = testing_menu_on_exit,
    .on_event = NULL,
};
