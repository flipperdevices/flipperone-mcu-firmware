#include <furi.h>
#include <furi_hal_i2c_config.h>
#include <gui/gui.h>
#include <gui/clay_helper.h>
#include <drivers/bq25792/bq25792.h>
#include <led/led_batch.h>

#define TAG              "PowerMenu"
#define POWER_MENU_ID(x) CLAY_SIDI(CLAY_STRING("PowerMenu"), x)

typedef enum {
    PowerMenuActionLeds,
    PowerMenuActionBacklight,
    PowerMenuActionPowerOff,
    PowerMenuActionReboot,
    PowerMenuActionCancel,
} PowerMenuAction;

static const char* power_menu_items[] = {
    [PowerMenuActionLeds] = "LEDs",
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
    const char* led_text;
} PowerMenuModel;

typedef struct {
    Gui* gui;
    View* view;
    FuriEventLoop* event_loop;
    Bq25792* bq25792;
    size_t selected_led_batch_index;
    size_t selected_backlight_index;
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

static const int8_t backlight_brightness_levels[] = {
    0,
    2,
    5,
    20,
    50,
    75,
    100,
};

static const size_t backlight_brightness_levels_count = COUNT_OF(backlight_brightness_levels);

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
            .clip = {.vertical = true, .childOffset = Clay_GetScrollOffset()},
            .floating =
                {
                    .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER, .parent = CLAY_ATTACH_POINT_CENTER_CENTER},
                    .attachTo = CLAY_ATTACH_TO_ROOT,
                },
            .border = {.color = COLOR_BLACK, .width = {.top = 1, .left = 1, .right = 1, .bottom = 1}},
            .cornerRadius = CLAY_CORNER_RADIUS(4),
        }) {
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
                if(i == PowerMenuActionBacklight) {
                    CLAY_TEXT(
                        clay_helper_string_from(model->backlight_text),
                        CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = selected ? COLOR_WHITE : COLOR_BLACK}));
                } else if(i == PowerMenuActionLeds) {
                    CLAY_TEXT(
                        clay_helper_string_from_chars(model->led_text),
                        CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = selected ? COLOR_WHITE : COLOR_BLACK}));
                }
            }
        }
    }

    return false;
}

static bool power_menu_model_init(PowerMenuModel* model, void* context) {
    model->backlight_text = furi_string_alloc();
    model->led_text = led_batch_names[0];
    return false;
}

static bool power_menu_model_deinit(PowerMenuModel* model, void* context) {
    furi_string_free(model->backlight_text);
    model->backlight_text = NULL;
    return false;
}

static bool power_menu_model_set_backlight_text(PowerMenuModel* model, void* context) {
    int8_t* brightness = context;
    furi_string_printf(model->backlight_text, " %d%%", *brightness);
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
    return true;
}

static void power_menu_model_apply(PowerMenu* instance, bool (*callback)(PowerMenuModel* model, void* context), void* context) {
    bool update;
    with_view_model(instance->view, PowerMenuModel * model, { update = callback(model, context); }, update);
}

static void power_menu_apply_backlight(PowerMenu* instance) {
    int8_t brightness = backlight_brightness_levels[instance->selected_backlight_index];
    gui_set_backlight(instance->gui, brightness);
    power_menu_model_apply(instance, power_menu_model_set_backlight_text, &brightness);
}

static void power_menu_input_menu(PowerMenu* instance, size_t selected_index) {
    switch(selected_index) {
    case PowerMenuActionBacklight:
        instance->selected_backlight_index = (instance->selected_backlight_index + 1) % backlight_brightness_levels_count;
        power_menu_apply_backlight(instance);
        break;
    case PowerMenuActionLeds:
        instance->selected_led_batch_index = (instance->selected_led_batch_index + 1) % led_batch_count;
        led_set_color_batch_simple(items[instance->selected_led_batch_index]);
        power_menu_model_apply(instance, power_menu_model_set_led_text, &instance->selected_led_batch_index);
        break;
    case PowerMenuActionPowerOff:
        bq25792_set_power_switch(instance->bq25792, Bq25792PowerShipMode);
        break;
    case PowerMenuActionReboot:
        bq25792_set_power_switch(instance->bq25792, Bq25792PowerReset);
        break;
    case PowerMenuActionCancel:
        power_menu_model_apply(instance, power_menu_input_menu_hide, NULL);
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

static PowerMenu* power_menu_alloc(void) {
    PowerMenu* instance = malloc(sizeof(PowerMenu));
    instance->bq25792 = bq25792_init(&furi_hal_i2c_handle_external, BQ25792_ADDRESS, NULL);
    instance->gui = furi_record_open(RECORD_GUI);
    instance->event_loop = furi_event_loop_alloc();

    instance->view = view_alloc();
    view_set_transparent(instance->view, true);
    view_allocate_model(instance->view, ViewModelTypeLockFree, sizeof(PowerMenuModel));
    power_menu_model_apply(instance, power_menu_model_init, NULL);
    view_set_layout_callback(instance->view, power_menu_layout);
    view_set_input_callback(instance->view, power_menu_input, instance);
    gui_add_view(instance->gui, instance->view, GuiViewPriorityMenu);
    instance->selected_backlight_index = 3; // 20%
    power_menu_apply_backlight(instance);
    return instance;
}

static void power_menu_free(PowerMenu* instance) {
    gui_remove_view(instance->gui, instance->view);
    furi_record_close(RECORD_GUI);
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
