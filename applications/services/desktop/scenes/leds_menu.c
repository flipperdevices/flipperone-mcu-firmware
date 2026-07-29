#include "../desktop_i.h"
#include "../scene.h"
#include <gui/modules/menu.h>
#include <led/led.h>
#include <led/led_batch.h>

#define TAG "LedsMenu"

typedef enum {
    LedsMenuItemLeds = 0,
    LedsMenuItemLinkBrightness,
    LedsMenuItemPowerBrightness,
    LedsMenuItemWattmeterBrightness,
} LedsMenuItem;

typedef struct {
    Desktop* desktop;
    Menu* menu;
} LedsMenuData;

static const uint8_t brightness_values[] = {0, 2, 5, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
static const struct {
    const char* text;
    const LedBatch* items;
} led_batches[] = {
    {"Off", &led_batch_all_off},
    {"Pw Red", &led_batch_power_red},
    {"On", &led_batch_all_on},
    {"White", &led_batch_all_white},
};

static void leds_menu_callback(MenuItem* item, size_t item_id, void* context) {
    furi_check(context);
    Scene* scene = context;
    LedsMenuData* scene_data = scene_get_data(scene);

    if(item == NULL) {
        desktop_send_scene_event(scene_data->desktop, DesktopSceneEventTypeEnterTestingMenu, scene);
    }
}

static void led_batches_select_callback(MenuItem* item, size_t selector_index, void* context) {
    furi_check(context);
    Scene* scene = context;
    LedsMenuData* scene_data = scene_get_data(scene);

    led_set_color_batch_simple(led_batches[selector_index].items);
    menu_item_selector_set_value(scene_data->menu, item, selector_index, led_batches[selector_index].text);
}

static void brightness_set_value(Menu* menu, MenuItem* item, uint8_t* value) {
    size_t nearest_index = 0;
    for(size_t i = 1; i < COUNT_OF(brightness_values); i++) {
        if(abs((int)brightness_values[i] - *value) <= abs((int)brightness_values[nearest_index] - *value)) {
            nearest_index = i;
        }
    }
    *value = brightness_values[nearest_index];
    FuriString* text = furi_string_alloc_printf("%u%%", *value);
    menu_item_selector_set_value(menu, item, nearest_index, furi_string_get_cstr(text));
    furi_string_free(text);
}

static void brightness_lnk_select_callback(MenuItem* item, size_t selector_index, void* context) {
    LedsMenuData* scene_data = scene_get_data(context);
    uint8_t brightness = brightness_values[selector_index];
    brightness_set_value(scene_data->menu, item, &brightness);
    Led* led = furi_record_open(RECORD_LEDS);
    led_set_brightness(led, LedGroupLink, brightness * 255 / 100);
    furi_record_close(RECORD_LEDS);
}

static void brightness_pwr_select_callback(MenuItem* item, size_t selector_index, void* context) {
    LedsMenuData* scene_data = scene_get_data(context);
    uint8_t brightness = brightness_values[selector_index];
    brightness_set_value(scene_data->menu, item, &brightness);
    Led* led = furi_record_open(RECORD_LEDS);
    led_set_brightness(led, LedGroupPower, brightness * 255 / 100);
    furi_record_close(RECORD_LEDS);
}

static void brightness_wtm_select_callback(MenuItem* item, size_t selector_index, void* context) {
    LedsMenuData* scene_data = scene_get_data(context);
    uint8_t brightness = brightness_values[selector_index];
    brightness_set_value(scene_data->menu, item, &brightness);
    Led* led = furi_record_open(RECORD_LEDS);
    led_set_brightness(led, LedGroupWattmeter, brightness * 255 / 100);
    furi_record_close(RECORD_LEDS);
}

static void leds_menu_on_enter(Scene* scene, void* context) {
    View* view = scene_get_view(scene);
    LedsMenuData* scene_data = malloc(sizeof(LedsMenuData));
    scene_data->desktop = context;
    scene_set_data(scene, scene_data);

    scene_data->menu = menu_alloc(view);
    menu_set_callback(scene_data->menu, leds_menu_callback, scene);
    menu_set_title(scene_data->menu, "> Settings > Testing > LEDs");

    Led* led = furi_record_open(RECORD_LEDS);
    uint8_t brightness = 0;

    MenuItem* item = menu_add_item(scene_data->menu, "LEDs", LedsMenuItemLeds, MenuItemSubTypeSelector);
    menu_item_selector_configure(scene_data->menu, item, COUNT_OF(led_batches), led_batches_select_callback);
    menu_item_selector_set_value(scene_data->menu, item, 0, led_batches[0].text);

    item = menu_add_item(scene_data->menu, "Lnk Led", LedsMenuItemLinkBrightness, MenuItemSubTypeSelector);
    menu_item_selector_configure(scene_data->menu, item, COUNT_OF(brightness_values), brightness_lnk_select_callback);
    furi_state_get(led_get_brightness_state(led, LedGroupLink), &brightness);
    brightness = brightness * 100 / 255;
    brightness_set_value(scene_data->menu, item, &brightness);

    item = menu_add_item(scene_data->menu, "Pwr Led", LedsMenuItemPowerBrightness, MenuItemSubTypeSelector);
    menu_item_selector_configure(scene_data->menu, item, COUNT_OF(brightness_values), brightness_pwr_select_callback);
    furi_state_get(led_get_brightness_state(led, LedGroupPower), &brightness);
    brightness = brightness * 100 / 255;
    brightness_set_value(scene_data->menu, item, &brightness);

    item = menu_add_item(scene_data->menu, "Wtm Led", LedsMenuItemWattmeterBrightness, MenuItemSubTypeSelector);
    menu_item_selector_configure(scene_data->menu, item, COUNT_OF(brightness_values), brightness_wtm_select_callback);
    furi_state_get(led_get_brightness_state(led, LedGroupWattmeter), &brightness);
    brightness = brightness * 100 / 255;
    brightness_set_value(scene_data->menu, item, &brightness);

    furi_record_close(RECORD_LEDS);
}

static void leds_menu_on_exit(Scene* scene, void* context) {
    UNUSED(context);
    LedsMenuData* scene_data = scene_get_data(scene);
    menu_free(scene_data->menu);
    free(scene_data);
    scene_set_data(scene, NULL);
}

const SceneCallbacks scene_leds_menu_callbacks = {
    .on_alloc = NULL,
    .on_enter = leds_menu_on_enter,
    .on_exit = leds_menu_on_exit,
    .on_event = NULL,
};
