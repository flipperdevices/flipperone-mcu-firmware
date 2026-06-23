#include "power_menu.h"
#include "input/input.h"
#include <furi.h>
#include <furi_hal_i2c_config.h>
#include <gui/gui.h>
#include <gui/clay_helper.h>
#include <led/led_batch.h>
#include <power/power.h>
#include <m-array.h>
#include <api_lock.h>

#include <desktop/desktop.h>

extern int32_t cpu_app(void* p);

#define TAG                     "PowerMenu"
#define POWER_MENU_MAX_MESSAGES (8)
#define POWER_MENU_ID(x)        CLAY_SIDI(CLAY_STRING("PowerMenu"), x)

#define POWER_MENU_CPU_APP_START   "CPU Start"
#define POWER_MENU_CPU_APP_MASKROM "CPU Maskrom"

typedef enum {
    PowerMenuCpuAppStart,
    PowerMenuCpuAppMaskrom,
    PowerMenuCpuAppCount
} PowerMenuCpuApp;

static const FlipperInternalApplication app[] = {
    [PowerMenuCpuAppStart] =
        {
            .app = cpu_app,
            .name = "CPU Start",
            .appid = "cpu",
            .stack_size = 4096,
            .flags = FlipperInternalApplicationFlagDefault,
            .args = "CPU Start",
        },
    [PowerMenuCpuAppMaskrom] =
        {
            .app = cpu_app,
            .name = "CPU Maskrom",
            .appid = "cpu",
            .stack_size = 4096,
            .flags = FlipperInternalApplicationFlagDefault,
            .args = "CPU Maskrom",
        },
};

typedef enum {
    PowerMenuActionLeds,
    PowerMenuActionLinkLedBrightness,
    PowerMenuActionPowerLedBrightness,
    PowerMenuActionWattmeterLedBrightness,
    PowerMenuActionBacklight,
    PowerMenuActionPowerOff,
    PowerMenuActionReboot,
    PowerMenuActionCancel,
} PowerMenuAction;

static const char* power_menu_items[] = {
    [PowerMenuActionLeds] = "LEDs",
    [PowerMenuActionLinkLedBrightness] = "Lnk Led",
    [PowerMenuActionPowerLedBrightness] = "Pwr Led",
    [PowerMenuActionWattmeterLedBrightness] = "Wtm Led",
    [PowerMenuActionBacklight] = "Backlight",
    [PowerMenuActionPowerOff] = "Power Off",
    [PowerMenuActionReboot] = "Reboot",
    [PowerMenuActionCancel] = "Cancel",
};

typedef struct {
    const char* text;
    PowerMenuClickWithContext on_click;
} PowerMenuCallbacks;

ARRAY_DEF(PowerMenuArray, PowerMenuCallbacks, M_POD_OPLIST);
#define M_OPL_PowerMenuArray_t() ARRAY_OPLIST(PowerMenuArray, M_POD_OPLIST)

typedef struct {
    PowerMenuArray_t data;
} PowerMenuStruct;

typedef enum {
    PowerMenuMessageTypeAddItem,
    PowerMenuMessageTypeRemoveItem,
} PowerMenuMessageType;

typedef struct {
    PowerMenuMessageType type;
    FuriApiLock lock;
    bool* result;
    union {
        struct {
            const char* text;
            PowerMenuClickWithContext on_click;
        } add_item;
        struct {
            const char* text;
        } remove_item;
    } as;
} PowerMenuMessage;

typedef struct {
    bool visible;
    size_t selected_index;
    FuriString* backlight_text;
    FuriString* link_led_brightness_text;
    FuriString* power_led_brightness_text;
    FuriString* wattmeter_led_brightness_text;
    const char* led_text;
    size_t power_menu_items_count;
    PowerMenuStruct* menu_items;
} PowerMenuModel;

typedef struct {
    Gui* gui;
    Led* led;
    View* view;
    FuriEventLoop* event_loop;
    size_t selected_led_batch_index;
    size_t selected_backlight_brightness_index;
    size_t selected_link_led_brightness_index;
    size_t selected_power_led_brightness_index;
    size_t selected_wattmeter_led_brightness_index;
    FuriMessageQueue* message_queue;
    bool app_running;
    bool app_add_item;
} PowerMenu;

static const LedBatch* items[] = {
    &led_batch_all_off,
    &led_batch_power_red,
    &led_batch_all_on,
    &led_batch_all_white,
};

static const char* led_batch_names[] = {
    " Off",
    " Pw Red",
    " On",
    " White",
};

static_assert(COUNT_OF(items) == COUNT_OF(led_batch_names), "items and led_batch_names count mismatch");

static const size_t led_batch_count = COUNT_OF(items);

static const uint8_t brightness_levels[] = {
    0,
    2,
    5,
    20,
    50,
    75,
    100,
};

static const size_t brightness_levels_count = COUNT_OF(brightness_levels);

static const uint8_t led_brightness_levels[] = {
    0,
    5,
    13,
    26,
    52,
    128,
    192,
    255,
};

static const size_t led_brightness_levels_count = COUNT_OF(led_brightness_levels);

static size_t led_brightness_get_nearest_index(uint8_t value) {
    size_t nearest_index = 0;
    for(size_t i = 1; i < led_brightness_levels_count; i++) {
        if(abs((int)led_brightness_levels[i] - value) < abs((int)led_brightness_levels[nearest_index] - value)) {
            nearest_index = i;
        }
    }
    return nearest_index;
}

static const char* power_menu_item_get_text(PowerMenuModel* model, size_t index) {
    PowerMenuCallbacks* item = PowerMenuArray_get(model->menu_items->data, index);
    return item ? item->text : NULL;
}

static size_t power_menu_item_get_count(PowerMenuModel* model) {
    return PowerMenuArray_size(model->menu_items->data);
}

static bool power_menu_layout(void* _model) {
    furi_assert(_model);
    PowerMenuModel* model = (PowerMenuModel*)_model;

    if(!model->visible) {
        return false;
    }

    CLAY(
        CLAY_APP_ID("Container"),
        {
            .backgroundColor = COLOR_WHITE,
            .layout =
                {
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0)},
                    .childGap = 4,
                    .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                    .padding = {4, 4, 4, 4},
                },
            .floating =
                {
                    .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER, .parent = CLAY_ATTACH_POINT_CENTER_CENTER},
                    .attachTo = CLAY_ATTACH_TO_ROOT,
                },
            .border = {.color = COLOR_BLACK, .width = {.top = 1, .left = 1, .right = 1, .bottom = 1}},
            .cornerRadius = CLAY_CORNER_RADIUS(4),
        }) {
        // Header
        CLAY_AUTO_ID({
            .layout =
                {
                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(13)},
                    .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                },
            .backgroundColor = COLOR_WHITE,
            .border = {.color = COLOR_BLACK, .width = {.bottom = 1}},
        }) {
            CLAY_TEXT(CLAY_STRING("Power"), CLAY_TEXT_CONFIG({.fontId = FontButton, .textColor = COLOR_BLACK}));
        }

        CLAY(
            CLAY_APP_ID("MenuContainer"),
            {
                .backgroundColor = COLOR_WHITE,
                .layout =
                    {
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        .sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIXED(100)},
                        .childGap = 4,
                        .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                    },
                .clip = {.vertical = true, .childOffset = Clay_GetScrollOffset()},
            }) {
            // Menu items
            for(uint32_t i = 0; i < model->power_menu_items_count; i++) {
                bool selected = (i == model->selected_index);

                // add a divider between built-in menu items and custom menu items
                if(power_menu_item_get_count(model) > 0 && i == power_menu_item_get_count(model)) {
                    CLAY_AUTO_ID({
                        .layout =
                            {
                                .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)},
                            },
                        .border = {.color = COLOR_BLACK, .width = {.bottom = 1}},
                    }) {
                    }
                }

                CLAY(
                    POWER_MENU_ID(i),
                    {
                        .layout =
                            {
                                .sizing = {.width = CLAY_SIZING_FIXED(80), .height = CLAY_SIZING_FIXED(13)},
                                .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                            },
                        .backgroundColor = selected ? COLOR_BLACK : COLOR_WHITE,
                        .cornerRadius = CLAY_CORNER_RADIUS(2),
                    }) {
                    CLAY_TEXT(
                        clay_helper_string_from_chars(
                            i < power_menu_item_get_count(model) ? power_menu_item_get_text(model, i) :
                                                                   (power_menu_items[i - power_menu_item_get_count(model)])),
                        CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = selected ? COLOR_WHITE : COLOR_BLACK}));

                    switch(i - power_menu_item_get_count(model)) {
                    case PowerMenuActionLeds:
                        CLAY_TEXT(
                            clay_helper_string_from_chars(model->led_text),
                            CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = selected ? COLOR_WHITE : COLOR_BLACK}));
                        break;
                    case PowerMenuActionLinkLedBrightness:
                        CLAY_TEXT(
                            clay_helper_string_from(model->link_led_brightness_text),
                            CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = selected ? COLOR_WHITE : COLOR_BLACK}));
                        break;
                    case PowerMenuActionPowerLedBrightness:
                        CLAY_TEXT(
                            clay_helper_string_from(model->power_led_brightness_text),
                            CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = selected ? COLOR_WHITE : COLOR_BLACK}));
                        break;
                    case PowerMenuActionWattmeterLedBrightness:
                        CLAY_TEXT(
                            clay_helper_string_from(model->wattmeter_led_brightness_text),
                            CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = selected ? COLOR_WHITE : COLOR_BLACK}));
                        break;
                    case PowerMenuActionBacklight:
                        CLAY_TEXT(
                            clay_helper_string_from(model->backlight_text),
                            CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = selected ? COLOR_WHITE : COLOR_BLACK}));
                        break;
                    default:
                        break;
                    }
                }
            }
        }
    }
    return false;
}

static bool power_menu_post_layout(void* _model) {
    PowerMenuModel* model = _model;
    furi_check(model);

    if(!model->visible) {
        return false;
    }

    Clay_ElementId scrollContainerId = CLAY_APP_ID("MenuContainer");
    Clay_ElementId targetChildId = POWER_MENU_ID(model->selected_index);

    if(clay_helper_scroll_to_child(scrollContainerId, targetChildId, 0, 0, 15)) {
        return true;
    }

    return false;
}

static bool power_menu_model_init(PowerMenuModel* model, void* context) {
    model->backlight_text = furi_string_alloc();
    model->link_led_brightness_text = furi_string_alloc();
    model->power_led_brightness_text = furi_string_alloc();
    model->wattmeter_led_brightness_text = furi_string_alloc();
    model->led_text = led_batch_names[0];
    model->power_menu_items_count = COUNT_OF(power_menu_items);

    model->menu_items = malloc(sizeof(PowerMenuStruct));
    PowerMenuArray_init(model->menu_items->data);

    return false;
}

static void power_menu_add_item(PowerMenuModel* model, const char* text, PowerMenuClickWithContext on_click) {
    PowerMenuCallbacks* item = PowerMenuArray_push_raw(model->menu_items->data);
    item->text = text;
    item->on_click = on_click;
    model->power_menu_items_count++;
}

static bool power_menu_remove_item(PowerMenuModel* model, const char* text) {
    size_t index = 0;
    bool found = false;
    for
        M_EACH(item, model->menu_items->data, PowerMenuArray_t) {
            if(strcmp(item->text, text) == 0) {
                item->text = NULL;
                item->on_click.callback = NULL;
                found = true;
                break;
            }
            index++;
        }
    if(found) {
        PowerMenuArray_erase(model->menu_items->data, index);
        model->power_menu_items_count--;
    }
    return found;
}

static bool power_menu_model_set_backlight_text(PowerMenuModel* model, void* context) {
    uint8_t* brightness = context;
    furi_string_printf(model->backlight_text, " %d%%", *brightness);
    return true;
}

static bool power_menu_model_set_link_led_brightness_text(PowerMenuModel* model, void* context) {
    uint8_t* brightness = context;
    furi_string_printf(model->link_led_brightness_text, " %d%%", (uint8_t)(*brightness / (255.0f / 100.0f)));
    return true;
}

static bool power_menu_model_set_power_led_brightness_text(PowerMenuModel* model, void* context) {
    uint8_t* brightness = context;
    furi_string_printf(model->power_led_brightness_text, " %d%%", (uint8_t)(*brightness / (255.0f / 100.0f)));
    return true;
}

static bool power_menu_model_set_wattmeter_led_brightness_text(PowerMenuModel* model, void* context) {
    uint8_t* brightness = context;
    furi_string_printf(model->wattmeter_led_brightness_text, " %d%%", (uint8_t)(*brightness / (255.0f / 100.0f)));
    return true;
}

static bool power_menu_model_set_led_text(PowerMenuModel* model, void* context) {
    size_t* led_batch_index = context;
    model->led_text = led_batch_names[*led_batch_index];
    return true;
}

static bool power_menu_model_menu_next(PowerMenuModel* model, void* context) {
    model->selected_index = (model->selected_index + 1) % model->power_menu_items_count;
    return true;
}

static bool power_menu_model_menu_prev(PowerMenuModel* model, void* context) {
    model->selected_index = (model->selected_index - 1 + model->power_menu_items_count) % model->power_menu_items_count;
    return true;
}

static bool power_menu_input_menu_get_selected_index(PowerMenuModel* model, void* context) {
    furi_check(context);
    int* selected_index = context;

    if(model->selected_index < power_menu_item_get_count(model)) {
        *selected_index = -((int)model->selected_index) - 1;
    } else {
        *selected_index = (int)(model->selected_index - power_menu_item_get_count(model));
    }

    return false;
}

static bool power_menu_input_menu_get_visible(PowerMenuModel* model, void* context) {
    furi_check(context);
    bool* visible = context;
    *visible = model->visible;
    return false;
}

static bool power_menu_input_menu_show(PowerMenuModel* model, void* context) {
    model->visible = true;
    return true;
}

static bool power_menu_input_menu_hide(PowerMenuModel* model, void* context) {
    model->visible = false;
    model->selected_index = 0;
    return true;
}

static void power_menu_model_apply(PowerMenu* instance, bool (*callback)(PowerMenuModel* model, void* context), void* context) {
    bool update;
    with_view_model(instance->view, PowerMenuModel * model, { update = callback(model, context); }, update);
}

static void power_menu_apply_backlight(PowerMenu* instance) {
    uint8_t brightness = brightness_levels[instance->selected_backlight_brightness_index];
    gui_set_backlight(instance->gui, brightness);
    power_menu_model_apply(instance, power_menu_model_set_backlight_text, &brightness);
}

#define POWER_MENU_PREV(var, min) (var = (var - 1 + min) % min)

static void power_menu_input_left(PowerMenu* instance, int selected_index) {
    switch(selected_index) {
    case PowerMenuActionLinkLedBrightness:
        POWER_MENU_PREV(instance->selected_link_led_brightness_index, led_brightness_levels_count);
        uint8_t link_brightness = led_brightness_levels[instance->selected_link_led_brightness_index];
        led_set_brightness(instance->led, LedGroupLink, link_brightness);
        break;
    case PowerMenuActionPowerLedBrightness:
        POWER_MENU_PREV(instance->selected_power_led_brightness_index, led_brightness_levels_count);
        uint8_t power_brightness = led_brightness_levels[instance->selected_power_led_brightness_index];
        led_set_brightness(instance->led, LedGroupPower, power_brightness);
        break;
    case PowerMenuActionWattmeterLedBrightness:
        POWER_MENU_PREV(instance->selected_wattmeter_led_brightness_index, led_brightness_levels_count);
        uint8_t wattmeter_brightness = led_brightness_levels[instance->selected_wattmeter_led_brightness_index];
        led_set_brightness(instance->led, LedGroupWattmeter, wattmeter_brightness);
        break;
    case PowerMenuActionBacklight:
        POWER_MENU_PREV(instance->selected_backlight_brightness_index, brightness_levels_count);
        power_menu_apply_backlight(instance);
        break;
    case PowerMenuActionLeds:
        POWER_MENU_PREV(instance->selected_led_batch_index, led_batch_count);
        led_set_color_batch_simple(items[instance->selected_led_batch_index]);
        power_menu_model_apply(instance, power_menu_model_set_led_text, &instance->selected_led_batch_index);
        break;
    default:
        break;
    }
}

#define POWER_MENU_NEXT(var, max) (var = (var + 1) % max)

static void power_menu_input_right(PowerMenu* instance, int selected_index) {
    switch(selected_index) {
    case PowerMenuActionLinkLedBrightness:
        POWER_MENU_NEXT(instance->selected_link_led_brightness_index, led_brightness_levels_count);
        uint8_t link_brightness = led_brightness_levels[instance->selected_link_led_brightness_index];
        led_set_brightness(instance->led, LedGroupLink, link_brightness);
        break;
    case PowerMenuActionPowerLedBrightness:
        POWER_MENU_NEXT(instance->selected_power_led_brightness_index, led_brightness_levels_count);
        uint8_t power_brightness = led_brightness_levels[instance->selected_power_led_brightness_index];
        led_set_brightness(instance->led, LedGroupPower, power_brightness);
        break;
    case PowerMenuActionWattmeterLedBrightness:
        POWER_MENU_NEXT(instance->selected_wattmeter_led_brightness_index, led_brightness_levels_count);
        uint8_t wattmeter_brightness = led_brightness_levels[instance->selected_wattmeter_led_brightness_index];
        led_set_brightness(instance->led, LedGroupWattmeter, wattmeter_brightness);
        break;
    case PowerMenuActionBacklight:
        POWER_MENU_NEXT(instance->selected_backlight_brightness_index, brightness_levels_count);
        power_menu_apply_backlight(instance);
        break;
    case PowerMenuActionLeds:
        POWER_MENU_NEXT(instance->selected_led_batch_index, led_batch_count);
        led_set_color_batch_simple(items[instance->selected_led_batch_index]);
        power_menu_model_apply(instance, power_menu_model_set_led_text, &instance->selected_led_batch_index);
        break;
    default:
        break;
    }
}

static void power_menu_input_menu(PowerMenu* instance, int selected_index, bool pressed) {
    // OK on menu does the same thing as right
    if(selected_index >= 0) {
        if(pressed) {
            power_menu_input_right(instance, selected_index);

            switch(selected_index) {
            case PowerMenuActionPowerOff: {
                Power* power_off = furi_record_open(RECORD_POWER);
                power_bq25792_set_power_switch(power_off, Bq25792PowerShipMode);
                furi_record_close(RECORD_POWER);
                break;
            }
            case PowerMenuActionReboot: {
                Power* power_reset = furi_record_open(RECORD_POWER);
                power_bq25792_set_power_switch(power_reset, Bq25792PowerReset);
                furi_record_close(RECORD_POWER);
                break;
            }
            case PowerMenuActionCancel:
                power_menu_model_apply(instance, power_menu_input_menu_hide, NULL);
                break;
            default:
                break;
            }
        }
    } else {
        size_t custom_index = selected_index * -1 - 1;

        with_view_model(
            instance->view,
            PowerMenuModel * model,
            {
                PowerMenuCallbacks* item = PowerMenuArray_get(model->menu_items->data, custom_index);
                if(item && item->on_click.callback) {
                    item->on_click.callback(pressed, item->on_click.context);
                }
            },
            true);
        if(!pressed) {
            power_menu_model_apply(instance, power_menu_input_menu_hide, NULL);
        }
    }
}

// #### app
// static void power_menu_remove_app_menu_items(PowerMenu* instance) {
//     power_menu_remove_menu_item(POWER_MENU_CPU_APP_START);
//     power_menu_remove_menu_item(POWER_MENU_CPU_APP_MASKROM);
//     instance->app_running = false;
// }

static void power_menu_cpu_app_start_callback(bool pressed, void* context) {
    PowerMenu* instance = context;
    furi_check(instance);
    if(pressed) {
        instance->app_running = true;
        desktop_start_app(&app[PowerMenuCpuAppStart]);
       // power_menu_remove_app_menu_items(instance);
    }
}

static void power_menu_add_app_menu_items(PowerMenu* instance) {
    if(!instance->app_running && !instance->app_add_item) {
        power_menu_add_menu_item(POWER_MENU_CPU_APP_START, (PowerMenuClickWithContext){.callback = power_menu_cpu_app_start_callback, .context = instance});
        power_menu_add_menu_item(POWER_MENU_CPU_APP_MASKROM, (PowerMenuClickWithContext){.callback = power_menu_cpu_app_start_callback, .context = instance});
        instance->app_add_item = true;
    }
}
// ####

static bool power_menu_input(InputEvent* event, void* context) {
    furi_check(context);
    PowerMenu* instance = context;
    bool consumed = false;
    bool visible;
    power_menu_model_apply(instance, power_menu_input_menu_get_visible, &visible);

    if(visible) {
        if(event->type == InputTypePress) {
            if(event->key == InputKeyUp) {
                power_menu_model_apply(instance, power_menu_model_menu_prev, NULL);
            } else if(event->key == InputKeyDown) {
                power_menu_model_apply(instance, power_menu_model_menu_next, NULL);
            } else if(event->key == InputKeyLeft) {
                int selected_index;
                power_menu_model_apply(instance, power_menu_input_menu_get_selected_index, &selected_index);
                power_menu_input_left(instance, selected_index);
            } else if(event->key == InputKeyRight) {
                int selected_index;
                power_menu_model_apply(instance, power_menu_input_menu_get_selected_index, &selected_index);
                power_menu_input_right(instance, selected_index);
            } else if(event->key == InputKeyOk) {
                int selected_index;
                power_menu_model_apply(instance, power_menu_input_menu_get_selected_index, &selected_index);
                power_menu_input_menu(instance, selected_index, true);
            } else if(event->key == InputKeyBack) {
                power_menu_model_apply(instance, power_menu_input_menu_hide, NULL);
            } else if(event->key == InputKey3) {
                power_menu_model_apply(instance, power_menu_input_menu_hide, NULL);
            }
        } else if(event->type == InputTypeRelease && event->key == InputKeyOk) {
            int selected_index;
            power_menu_model_apply(instance, power_menu_input_menu_get_selected_index, &selected_index);
            power_menu_input_menu(instance, selected_index, false);
        }

        // Consume all events when visible, except for release events
        if(event->type != InputTypeRelease) {
            consumed = true;
        }
    } else {
        if(event->type == InputTypePress) {
            if(event->key == InputKey3) {
                power_menu_model_apply(instance, power_menu_input_menu_show, NULL);
                power_menu_add_app_menu_items(instance);
                consumed = true;
            }
        }
    }

    return consumed;
}

static void power_menu_link_led_brightness_callback(const void* item, void* context) {
    uint8_t* brightness = (uint8_t*)item;
    PowerMenu* instance = context;
    instance->selected_link_led_brightness_index = led_brightness_get_nearest_index(*brightness);
    *brightness = led_brightness_levels[instance->selected_link_led_brightness_index];
    power_menu_model_apply(instance, power_menu_model_set_link_led_brightness_text, brightness);
}

static void power_menu_power_led_brightness_callback(const void* item, void* context) {
    uint8_t* brightness = (uint8_t*)item;
    PowerMenu* instance = context;
    instance->selected_power_led_brightness_index = led_brightness_get_nearest_index(*brightness);
    *brightness = led_brightness_levels[instance->selected_power_led_brightness_index];
    power_menu_model_apply(instance, power_menu_model_set_power_led_brightness_text, brightness);
}

static void power_menu_wattmeter_led_brightness_callback(const void* item, void* context) {
    uint8_t* brightness = (uint8_t*)item;
    PowerMenu* instance = context;
    instance->selected_wattmeter_led_brightness_index = led_brightness_get_nearest_index(*brightness);
    *brightness = led_brightness_levels[instance->selected_wattmeter_led_brightness_index];
    power_menu_model_apply(instance, power_menu_model_set_wattmeter_led_brightness_text, brightness);
}

static void power_menu_message_queue_callback(FuriEventLoopObject* object, void* context) {
    furi_assert(context);
    PowerMenu* instance = context;
    furi_assert(object == instance->message_queue);

    PowerMenuMessage msg;
    furi_check(furi_message_queue_get(instance->message_queue, &msg, 0) == FuriStatusOk);

    bool result = false;

    switch(msg.type) {
    case PowerMenuMessageTypeAddItem:
        with_view_model(instance->view, PowerMenuModel * model, { power_menu_add_item(model, msg.as.add_item.text, msg.as.add_item.on_click); }, true);
        result = true;
        break;
    case PowerMenuMessageTypeRemoveItem:
        with_view_model(instance->view, PowerMenuModel * model, { result = power_menu_remove_item(model, msg.as.remove_item.text); }, true);
        break;

    default:
        furi_crash("Invalid message type");
        break;
    }

    if(msg.result) {
        *msg.result = result;
    }

    if(msg.lock) {
        api_lock_unlock(msg.lock);
    }
}

static PowerMenu* power_menu_alloc(void) {
    PowerMenu* instance = malloc(sizeof(PowerMenu));
    instance->gui = furi_record_open(RECORD_GUI);
    instance->led = furi_record_open(RECORD_LEDS);
    instance->event_loop = furi_event_loop_alloc();
    instance->message_queue = furi_message_queue_alloc(POWER_MENU_MAX_MESSAGES, sizeof(PowerMenuMessage));

    instance->view = view_alloc();
    view_set_transparent(instance->view, true);
    view_allocate_model(instance->view, ViewModelTypeLockFree, sizeof(PowerMenuModel));
    power_menu_model_apply(instance, power_menu_model_init, NULL);
    view_set_layout_callback(instance->view, power_menu_layout);
    view_set_post_layout_callback(instance->view, power_menu_post_layout);
    view_set_input_callback(instance->view, power_menu_input, instance);
    gui_add_view(instance->gui, instance->view, GuiViewPriorityMenu);
    instance->selected_backlight_brightness_index = 3; // 20%
    power_menu_apply_backlight(instance);

    furi_state_subscribe(led_get_link_brightness_state(instance->led), power_menu_link_led_brightness_callback, instance);
    furi_state_subscribe(led_get_power_brightness_state(instance->led), power_menu_power_led_brightness_callback, instance);
    furi_state_subscribe(led_get_wattmeter_brightness_state(instance->led), power_menu_wattmeter_led_brightness_callback, instance);
    furi_event_loop_subscribe_message_queue(instance->event_loop, instance->message_queue, FuriEventLoopEventIn, power_menu_message_queue_callback, instance);

    furi_record_create(RECORD_POWER_MENU, instance);

    return instance;
}

int32_t power_menu_srv(void* p) {
    UNUSED(p);
    PowerMenu* instance = power_menu_alloc();
    furi_event_loop_run(instance->event_loop);
    furi_crash();
    return 0;
}

static void power_menu_send_message(PowerMenu* instance, const PowerMenuMessage* message) {
    furi_check(furi_message_queue_put(instance->message_queue, message, FuriWaitForever) == FuriStatusOk);

    if(message->lock) {
        api_lock_wait_unlock_and_free(message->lock);
    }
}

bool power_menu_add_menu_item(const char* text, PowerMenuClickWithContext on_click) {
    furi_assert(text);
    bool result;
    PowerMenuMessage msg = {
        .type = PowerMenuMessageTypeAddItem,
        .as.add_item =
            {
                .text = text,
                .on_click = on_click,
            },
        .result = &result,
        .lock = api_lock_alloc_locked(),
    };

    PowerMenu* instance = furi_record_open(RECORD_POWER_MENU);
    power_menu_send_message(instance, &msg);
    furi_record_close(RECORD_POWER_MENU);

    if(!result) {
        FURI_LOG_E(TAG, "Failed to add menu item");
    }
    return result;
}

bool power_menu_remove_menu_item(const char* text) {
    furi_assert(text);
    bool result;
    PowerMenuMessage msg = {
        .type = PowerMenuMessageTypeRemoveItem,
        .as.remove_item =
            {
                .text = text,
            },
        .result = &result,
        .lock = api_lock_alloc_locked(),
    };

    PowerMenu* instance = furi_record_open(RECORD_POWER_MENU);
    power_menu_send_message(instance, &msg);
    furi_record_close(RECORD_POWER_MENU);

    if(!result) {
        FURI_LOG_E(TAG, "Failed to remove menu item");
    }
    return result;
}
