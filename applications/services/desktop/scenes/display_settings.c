#include "../desktop_i.h"
#include "../scene.h"
#include <gui/modules/menu.h>
#include <led/led.h>

#define TAG "DisplaySettings"

typedef enum {
    DisplaySettingsItemBrightness = 0,
    DisplaySettingsItemTime,
} DisplaySettingsItem;

static const uint8_t brightness_values[] = {0, 2, 5, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
static const struct {
    const char* text;
    uint32_t value;
} backlight_time_values[] = {
    {"3 sec", 3},
    {"10 sec", 10},
    {"1 min", 60},
    {"Never", 0},
};

typedef struct {
    Desktop* desktop;
    Menu* menu;
    MenuItem* brightness_item;
    MenuItem* bl_time_item;
    FuriStateSub* brightness_state_sub;
    FuriStateSub* bl_time_state_sub;
    Led* led;
} DisplaySettingsData;

static void display_settings_menu_callback(MenuItem* item, size_t item_id, void* context) {
    furi_check(context);
    Scene* scene = context;
    DisplaySettingsData* scene_data = scene_get_data(scene);

    if(item == NULL) {
        FURI_LOG_I(TAG, "Exit");
        desktop_send_scene_event(scene_data->desktop, DesktopSceneEventTypeEnterSettingsMenu, scene);
    }
}

static void display_brightness_selector_update(DisplaySettingsData* scene_data, uint8_t* value) {
    size_t nearest_index = 0;
    for(size_t i = 1; i < COUNT_OF(brightness_values); i++) {
        if(abs((int)brightness_values[i] - *value) <= abs((int)brightness_values[nearest_index] - *value)) {
            nearest_index = i;
        }
    }
    uint8_t brightness = brightness_values[nearest_index];
    *value = brightness;

    FuriString* backlight_text = furi_string_alloc();
    if(brightness == 0) {
        furi_string_printf(backlight_text, "OFF");
    } else {
        furi_string_printf(backlight_text, "%u%%", brightness);
    }
    menu_item_selector_set_value(scene_data->menu, scene_data->brightness_item, nearest_index, furi_string_get_cstr(backlight_text));
    furi_string_free(backlight_text);
}

static void display_time_selector_update(DisplaySettingsData* scene_data, uint32_t* value) {
    size_t nearest_index = 0;
    for(size_t i = 1; i < COUNT_OF(backlight_time_values); i++) {
        if(abs((int)backlight_time_values[i].value - *value) <= abs((int)backlight_time_values[nearest_index].value - *value)) {
            nearest_index = i;
        }
    }
    uint32_t time = backlight_time_values[nearest_index].value;
    *value = time;

    menu_item_selector_set_value(scene_data->menu, scene_data->bl_time_item, nearest_index, backlight_time_values[nearest_index].text);
}

static void display_brightness_selector_callback(MenuItem* item, size_t selector_index, void* context) {
    furi_check(context);
    Scene* scene = context;
    DisplaySettingsData* scene_data = scene_get_data(scene);

    uint8_t brightness = brightness_values[selector_index];
    display_brightness_selector_update(scene_data, &brightness);
    led_set_brightness(scene_data->led, LedGroupDisplayBacklight, (uint32_t)brightness * 255 / 100);
}

static void display_time_selector_callback(MenuItem* item, size_t selector_index, void* context) {
    furi_check(context);
    Scene* scene = context;
    DisplaySettingsData* scene_data = scene_get_data(scene);

    uint32_t time = backlight_time_values[selector_index].value;
    display_time_selector_update(scene_data, &time);
    led_backlight_set_time(scene_data->led, time * 1000);
}

static void display_brightness_state_callback(const void* item, void* context) {
    uint8_t* brightness_temp = (uint8_t*)item;
    uint8_t brightness = (uint32_t)(*(uint8_t*)item) * 100 / 255;
    furi_check(context);
    DisplaySettingsData* scene_data = context;
    display_brightness_selector_update(scene_data, &brightness);
}

static void display_time_state_callback(const void* item, void* context) {
    uint32_t time = *(uint32_t*)item / 1000;
    furi_check(context);
    DisplaySettingsData* scene_data = context;
    display_time_selector_update(scene_data, &time);
}

static void display_settings_on_enter(Scene* scene, void* context) {
    View* view = scene_get_view(scene);
    DisplaySettingsData* scene_data = malloc(sizeof(DisplaySettingsData));
    scene_data->desktop = context;
    scene_data->led = furi_record_open(RECORD_LEDS);
    scene_set_data(scene, scene_data);

    scene_data->menu = menu_alloc(view);
    menu_set_callback(scene_data->menu, display_settings_menu_callback, scene);
    menu_set_title(scene_data->menu, "> Settings > Display");

    scene_data->brightness_item = menu_add_item(scene_data->menu, "Backlight", DisplaySettingsItemBrightness, MenuItemSubTypeSelector);
    menu_item_selector_configure(scene_data->menu, scene_data->brightness_item, COUNT_OF(brightness_values), display_brightness_selector_callback);
    scene_data->brightness_state_sub =
        furi_state_subscribe(led_get_brightness_state(scene_data->led, LedGroupDisplayBacklight), display_brightness_state_callback, scene_data);

    scene_data->bl_time_item = menu_add_item(scene_data->menu, "Backlight OFF when inactive", DisplaySettingsItemTime, MenuItemSubTypeSelector);
    menu_item_selector_configure(scene_data->menu, scene_data->bl_time_item, COUNT_OF(backlight_time_values), display_time_selector_callback);
    scene_data->bl_time_state_sub = furi_state_subscribe(led_get_backlight_time_state(scene_data->led), display_time_state_callback, scene_data);
}

static void display_settings_on_exit(Scene* scene, void* context) {
    UNUSED(context);
    DisplaySettingsData* scene_data = scene_get_data(scene);
    furi_state_unsubscribe(scene_data->brightness_state_sub);
    furi_state_unsubscribe(scene_data->bl_time_state_sub);
    furi_record_close(RECORD_LEDS);
    menu_free(scene_data->menu);
    free(scene_data);
    scene_set_data(scene, NULL);
}

const SceneCallbacks scene_display_settings_callbacks = {
    .on_alloc = NULL,
    .on_enter = display_settings_on_enter,
    .on_exit = display_settings_on_exit,
    .on_event = NULL,
};
