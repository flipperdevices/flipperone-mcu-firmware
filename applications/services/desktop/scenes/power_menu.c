#include <assets.h>
#include <gui/gui.h>
#include <gui/clay_helper.h>
#include <power/power.h>
#include "../scene.h"
#include "../elements.h"
#include "../desktop.h"
#include "../desktop_i.h"

extern int32_t cpu_app(void* p);

#define TAG              "PowerMenu"
#define POWER_MENU_ID(x) CLAY_SIDI(CLAY_STRING("PowerMenu"), x)

typedef struct {
    const char* text;
    void (*on_click)(Scene* scene);
    bool enabled;
} PowerMenuItem;

typedef struct {
    PowerMenuItem* selected_item;
} PowerMenuModel;

static void desktop_power_menu_start_linux(Scene* scene);
static void desktop_power_menu_start_maskrom(Scene* scene);
static void desktop_power_menu_power_off(Scene* scene);

static PowerMenuItem power_menu_items[] = {
    {.text = "Start Linux", .on_click = desktop_power_menu_start_linux, .enabled = true},
    {.text = "Maskrom", .on_click = desktop_power_menu_start_maskrom, .enabled = true},
    {.text = "Power Off", .on_click = desktop_power_menu_power_off, .enabled = true},
};

static const size_t power_menu_items_count = COUNT_OF(power_menu_items);

static const FlipperInternalApplication cpu_app_start = {
    .app = cpu_app,
    .name = "Start Linux",
    .appid = "cpu",
    .stack_size = 4096,
    .flags = FlipperInternalApplicationFlagDefault,
    .args = "start",
};

static const FlipperInternalApplication cpu_maskrom_app = {
    .app = cpu_app,
    .name = "Maskrom",
    .appid = "cpu",
    .stack_size = 4096,
    .flags = FlipperInternalApplicationFlagDefault,
    .args = "maskrom",
};

static void desktop_power_menu_start_linux(Scene* scene) {
    Desktop* desktop = scene_get_data(scene);
    desktop_start_app(&cpu_app_start);
    desktop_send_scene_event(desktop, DesktopSceneEventTypeTogglePowerMenu, NULL);
}

static void desktop_power_menu_start_maskrom(Scene* scene) {
    Desktop* desktop = scene_get_data(scene);
    desktop_start_app(&cpu_maskrom_app);
    desktop_send_scene_event(desktop, DesktopSceneEventTypeTogglePowerMenu, NULL);
}

static void desktop_power_menu_power_off(Scene* scene) {
    UNUSED(scene);

    Power* power_off = furi_record_open(RECORD_POWER);
    power_bq25792_set_power_switch(power_off, Bq25792PowerShipMode);
    furi_record_close(RECORD_POWER);
}

static bool power_menu_layout(void* _model) {
    furi_assert(_model);
    PowerMenuModel* model = _model;

    CLAY(
        CLAY_APP_ID("Background"),
        {
            .layout =
                {
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                    .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_BOTTOM},
                },
            .backgroundColor = (Clay_Color){0xFF, 0xFF, 0xFF, 0xFF / 2},
            .floating =
                {
                    .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_TOP, .parent = CLAY_ATTACH_POINT_LEFT_TOP},
                    .attachTo = CLAY_ATTACH_TO_ROOT,
                },
        }) {
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
                        .padding = {.top = 3, .bottom = 2},
                    },
                .floating =
                    {
                        .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER, .parent = CLAY_ATTACH_POINT_CENTER_CENTER},
                        .attachTo = CLAY_ATTACH_TO_ROOT,
                    },
                .border = {.color = COLOR_BLACK, .width = {.top = 1, .left = 1, .right = 1, .bottom = 1}},
                .cornerRadius = CLAY_CORNER_RADIUS(5),
            }) {
            CLAY(
                CLAY_APP_ID("ClipWrapper"),
                {
                    .backgroundColor = COLOR_WHITE,
                    .layout =
                        {
                            .layoutDirection = CLAY_TOP_TO_BOTTOM,
                            .sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0)},
                            .childGap = 1,
                            .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                        },
                    .clip = {.vertical = true, .childOffset = Clay_GetScrollOffset()},
                }) {
                // Menu items
                for(uint32_t i = 0; i < power_menu_items_count; i++) {
                    PowerMenuItem* item = &power_menu_items[i];
                    if(!item->enabled) {
                        continue;
                    }

                    bool selected = (model->selected_item == item);

                    Clay_TextElementConfig text_config = {
                        .fontId = FontBody,
                        .textColor = COLOR_BLACK,
                        .wrapMode = CLAY_TEXT_WRAP_NONE,
                    };

                    CLAY(
                        POWER_MENU_ID((size_t)item),
                        {
                            .layout =
                                {
                                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(16)},
                                    .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                                    .padding =
                                        {.left = 3 + (selected ? 0 : power_menu_border_left.width), .right = 3 + (selected ? 0 : power_menu_border_right.width)},
                                },
                        }) {
                        if(selected) {
                            clay_fixed_image(&power_menu_border_left);
                        }

                        CLAY_AUTO_ID({
                            .layout =
                                {
                                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(16)},
                                    .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                                    .padding = {.left = 7 - power_menu_border_left.width + 1, .right = 7 - power_menu_border_right.width + 1},
                                },
                            .border = {.color = COLOR_BLACK, .width = {.bottom = selected ? 2 : 0, .top = selected ? 1 : 0}},
                        }) {
                            CLAY_TEXT(clay_helper_string_from_chars(item->text), CLAY_TEXT_CONFIG(text_config));
                        }

                        if(selected) {
                            clay_fixed_image(&power_menu_border_right);
                        }
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

    Clay_ElementId scrollContainerId = CLAY_APP_ID("ClipWrapper");
    Clay_ElementId targetChildId = POWER_MENU_ID((size_t)model->selected_item);

    if(clay_helper_scroll_to_child(scrollContainerId, targetChildId, 0, 0, 15)) {
        return true;
    }

    return false;
}

static bool power_menu_model_menu_next(PowerMenuModel* model, void* context) {
    UNUSED(context);
    size_t index = model->selected_item - power_menu_items;
    for(size_t i = 0; i < power_menu_items_count; i++) {
        index = (index + 1) % power_menu_items_count;
        if(power_menu_items[index].enabled) {
            model->selected_item = &power_menu_items[index];
            break;
        }
    }
    return true;
}

static bool power_menu_model_menu_prev(PowerMenuModel* model, void* context) {
    UNUSED(context);
    size_t index = model->selected_item - power_menu_items;
    for(size_t i = 0; i < power_menu_items_count; i++) {
        index = (index + power_menu_items_count - 1) % power_menu_items_count;
        if(power_menu_items[index].enabled) {
            model->selected_item = &power_menu_items[index];
            break;
        }
    }
    return true;
}

static bool power_menu_input_menu_get_selected_item(PowerMenuModel* model, void* context) {
    furi_check(context);
    PowerMenuItem** selected_item = context;
    *selected_item = model->selected_item;
    return false;
}

static void power_menu_model_apply(View* view, bool (*callback)(PowerMenuModel* model, void* context), void* context) {
    bool update;
    with_view_model(view, PowerMenuModel * model, { update = callback(model, context); }, update);
}

static bool power_menu_model_init(PowerMenuModel* model, void* context) {
    UNUSED(context);
    size_t first_enabled_index = 0;
    for(size_t i = 0; i < power_menu_items_count; i++) {
        if(power_menu_items[i].enabled) {
            first_enabled_index = i;
            break;
        }
    }
    model->selected_item = &power_menu_items[first_enabled_index];

    return false;
}

static bool power_menu_input(InputEvent* event, void* context) {
    furi_check(context);
    Scene* scene = context;
    View* view = scene_get_view(scene);
    Desktop* desktop = scene_get_data(scene);

    furi_check(view);
    furi_check(desktop);

    bool consumed = false;

    if(event->type == InputTypePress) {
        if(event->key == InputKeyUp) {
            power_menu_model_apply(view, power_menu_model_menu_prev, NULL);
        } else if(event->key == InputKeyDown) {
            power_menu_model_apply(view, power_menu_model_menu_next, NULL);
        } else if(event->key == InputKeyOk) {
            PowerMenuItem* selected_item;
            power_menu_model_apply(view, power_menu_input_menu_get_selected_item, &selected_item);
            selected_item->on_click(scene);
        } else if(event->key == InputKeyBack) {
            desktop_send_scene_event(desktop, DesktopSceneEventTypeTogglePowerMenu, NULL);
        }
    }

    // Consume all events when visible, except for release events and pressing InputKey3
    if(event->type != InputTypeRelease && event->key != InputKey3) {
        consumed = true;
    }

    return consumed;
}

static void power_menu_on_alloc(Scene* scene, void* context) {
    Desktop* desktop = context;

    View* view = scene_get_view(scene);
    furi_check(view);

    view_allocate_model(view, ViewModelTypeLockFree, sizeof(PowerMenuModel));
    power_menu_model_apply(view, power_menu_model_init, NULL);
    view_set_layout_callback(view, power_menu_layout);
    view_set_post_layout_callback(view, power_menu_post_layout);
    view_set_input_callback(view, power_menu_input, scene);
    view_set_transparent(view, true);

    scene_set_data(scene, desktop);
}

static void power_menu_on_enter(Scene* scene, void* app) {
    View* view = scene_get_view(scene);
    power_menu_model_apply(view, power_menu_model_init, NULL);
}

const SceneCallbacks scene_power_menu_callbacks = {
    .on_alloc = power_menu_on_alloc,
    .on_enter = power_menu_on_enter,
    .on_exit = NULL,
    .on_event = NULL,
};
