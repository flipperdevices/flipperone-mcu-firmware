#include <furi.h>
#include <furi_hal_i2c_config.h>
#include <gui/gui.h>
#include <gui/clay_helper.h>
#include <led/led_batch.h>
#include <power/power.h>

#define TAG              "PowerMenu"
#define POWER_MENU_ID(x) CLAY_SIDI(CLAY_STRING("PowerMenu"), x)

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

static size_t power_menu_items_count = COUNT_OF(power_menu_items);

typedef struct {
    bool visible;
    size_t selected_index;
    FuriString* backlight_text;
    FuriString* link_led_brightness_text;
    FuriString* power_led_brightness_text;
    FuriString* wattmeter_led_brightness_text;
    const char* led_text;
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

static_assert(COUNT_OF(items) == COUNT_OF(led_batch_names));

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
            for(uint32_t i = 0; i < power_menu_items_count; i++) {
                bool selected = (i == model->selected_index);
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
                        clay_helper_string_from_chars(power_menu_items[i]),
                        CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = selected ? COLOR_WHITE : COLOR_BLACK}));

                    switch(i) {
                    case PowerMenuActionBacklight:
                        CLAY_TEXT(
                            clay_helper_string_from(model->backlight_text),
                            CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = selected ? COLOR_WHITE : COLOR_BLACK}));
                        break;
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
    return false;
}

static bool power_menu_model_deinit(PowerMenuModel* model, void* context) {
    furi_string_free(model->backlight_text);
    furi_string_free(model->link_led_brightness_text);
    furi_string_free(model->power_led_brightness_text);
    furi_string_free(model->wattmeter_led_brightness_text);
    model->backlight_text = NULL;
    model->link_led_brightness_text = NULL;
    model->power_led_brightness_text = NULL;
    model->wattmeter_led_brightness_text = NULL;
    model->led_text = NULL;
    return false;
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
    model->selected_index = (model->selected_index + 1) % power_menu_items_count;
    return true;
}

static bool power_menu_model_menu_prev(PowerMenuModel* model, void* context) {
    model->selected_index = (model->selected_index - 1 + power_menu_items_count) % power_menu_items_count;
    return true;
}

static bool power_menu_input_menu_get_selected_index(PowerMenuModel* model, void* context) {
    furi_check(context);
    size_t* selected_index = context;
    *selected_index = model->selected_index;
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

static void power_menu_input_left(PowerMenu* instance, size_t selected_index) {
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

static void power_menu_input_right(PowerMenu* instance, size_t selected_index) {
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

static void power_menu_input_menu(PowerMenu* instance, size_t selected_index) {
    // OK on menu does the same thing as right
    power_menu_input_right(instance, selected_index);

    switch(selected_index) {
    case PowerMenuActionPowerOff:
        Power* power_off = furi_record_open(RECORD_POWER);
        power_bq25792_set_power_switch(power_off, Bq25792PowerShipMode);
        furi_record_close(RECORD_POWER);
        break;
    case PowerMenuActionReboot:
        Power* power_reset = furi_record_open(RECORD_POWER);
        power_bq25792_set_power_switch(power_reset, Bq25792PowerReset);
        furi_record_close(RECORD_POWER);
        break;
    case PowerMenuActionCancel:
        power_menu_model_apply(instance, power_menu_input_menu_hide, NULL);
        break;
    default:
        break;
    }
}

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
                size_t selected_index;
                power_menu_model_apply(instance, power_menu_input_menu_get_selected_index, &selected_index);
                power_menu_input_left(instance, selected_index);
            } else if(event->key == InputKeyRight) {
                size_t selected_index;
                power_menu_model_apply(instance, power_menu_input_menu_get_selected_index, &selected_index);
                power_menu_input_right(instance, selected_index);
            } else if(event->key == InputKeyOk) {
                size_t selected_index;
                power_menu_model_apply(instance, power_menu_input_menu_get_selected_index, &selected_index);
                power_menu_input_menu(instance, selected_index);
            } else if(event->key == InputKeyBack) {
                power_menu_model_apply(instance, power_menu_input_menu_hide, NULL);
            } else if(event->key == InputKey3) {
                power_menu_model_apply(instance, power_menu_input_menu_hide, NULL);
            }
        }

        // Consume all events when visible, except for release events
        if(event->type != InputTypeRelease) {
            consumed = true;
        }
    } else {
        if(event->key == InputKey3) {
            if(event->type == InputTypeLong) {
                power_menu_model_apply(instance, power_menu_input_menu_show, NULL);
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

static PowerMenu* power_menu_alloc(void) {
    PowerMenu* instance = malloc(sizeof(PowerMenu));
    instance->gui = furi_record_open(RECORD_GUI);
    instance->led = furi_record_open(RECORD_LEDS);
    instance->event_loop = furi_event_loop_alloc();

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

    return instance;
}

static void power_menu_free(PowerMenu* instance) {
    gui_remove_view(instance->gui, instance->view);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_LEDS);

    power_menu_model_apply(instance, power_menu_model_deinit, NULL);
    view_free(instance->view);
    furi_event_loop_free(instance->event_loop);
    free(instance);
}

int32_t power_menu_srv(void* p) {
    UNUSED(p);
    PowerMenu* instance = power_menu_alloc();
    furi_event_loop_run(instance->event_loop);
    power_menu_free(instance);
    return 0;
}
