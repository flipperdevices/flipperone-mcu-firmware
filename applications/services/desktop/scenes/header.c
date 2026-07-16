#include <assets.h>
#include <version.h>
#include <gui/gui.h>
#include <gui/clay_helper.h>
#include "../scene.h"
#include <power/power.h>
#include "../desktop_i.h"

#define TAG "Header"

typedef struct {
    FuriString* version_text;
    FuriString* charge_text;
    float charge_indicator_width;
} HeaderViewModel;

static const Clay_Color COLOR_VERSION = {0x69, 0x69, 0x69, 255};

static bool header_layout(void* _model) {
    furi_assert(_model);
    HeaderViewModel* model = _model;

    CLAY(
        CLAY_APP_ID("Container"),
        {
            .backgroundColor = COLOR_WHITE,
            .layout =
                {
                    .layoutDirection = CLAY_LEFT_TO_RIGHT,
                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(11)},
                    .childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER},
                    .padding = {.left = 3, .right = 2, .top = 0, .bottom = 0},
                },
        }) {
        CLAY_AUTO_ID({
            .layout =
                {
                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                    .childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER},
                    .padding = {.left = 0, .right = 0, .top = 2, .bottom = 0},
                },
        }) {
            CLAY_TEXT(
                clay_helper_string_from(model->version_text),
                CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = COLOR_VERSION, .wrapMode = CLAY_TEXT_WRAP_NONE}));
        }
        CLAY_AUTO_ID({
            .layout =
                {
                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                    .childAlignment = {.x = CLAY_ALIGN_X_RIGHT, .y = CLAY_ALIGN_Y_CENTER},
                    .padding = {.left = 0, .right = 0, .top = 2, .bottom = 0},
                },
        }) {
            CLAY_TEXT(clay_helper_string_from(model->charge_text), CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = COLOR_BLACK}));
        }
        CLAY_AUTO_ID({
            .layout =
                {
                    .sizing =
                        {
                            .height = CLAY_SIZING_FIXED(header_battery.height),
                            .width = CLAY_SIZING_FIXED(header_battery.width),
                        },
                    .padding = {.left = 2, .right = 4, .top = 2, .bottom = 2},
                },
            .image = {.imageData = (void*)&header_battery},
        }) {
            CLAY_AUTO_ID({
                .backgroundColor = {0, 0, 0, 0xFF},
                .layout =
                    {
                        .sizing =
                            {
                                .width = CLAY_SIZING_PERCENT(model->charge_indicator_width),
                                .height = CLAY_SIZING_GROW(0),
                            },
                    },
            }){};
        }
    }
    return false;
}

static void header_update_battery_state(Scene* scene) {
    View* view = scene_get_view(scene);

    uint8_t battery_soc = 0;
    Power* power = furi_record_open(RECORD_POWER);
    power_bq28z620_get_relative_state_of_charge(power, &battery_soc);
    furi_record_close(RECORD_POWER);

    if(battery_soc > 100) battery_soc = 100;

    with_view_model(
        view,
        HeaderViewModel * model,
        {
            furi_string_printf(model->charge_text, "%u%%", battery_soc);
            model->charge_indicator_width = (float)battery_soc / 100.f;
        },
        true);
}

static void header_on_alloc(Scene* scene, void* context) {
    UNUSED(context);

    View* view = scene_get_view(scene);

    view_allocate_model(view, ViewModelTypeLockFree, sizeof(HeaderViewModel));
    view_set_layout_callback(view, header_layout);

    with_view_model(
        view,
        HeaderViewModel * model,
        {
            const Version* version = version_get();

            model->version_text = furi_string_alloc();
            furi_string_printf(model->version_text, "%s %s", version_get_gitbranch(version), version_get_githash(version));

            model->charge_text = furi_string_alloc();
        },
        false);
}

static void header_on_show(Scene* scene, void* context) {
    UNUSED(context);
    header_update_battery_state(scene);
}

static bool header_on_event(Scene* scene, uint32_t event, void* data) {
    UNUSED(data);
    bool consumed = false;

    if(event == DesktopSceneEventTypePowerUpdate) {
        // header_update_battery_state(scene); // Disabled for now
        consumed = true;
    }
    return consumed;
}

const SceneCallbacks scene_header_callbacks = {
    .on_alloc = header_on_alloc,
    .on_enter = header_on_show,
    .on_exit = NULL,
    .on_event = header_on_event,
};
