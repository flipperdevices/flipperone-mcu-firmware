#include <assets.h>
#include <version.h>
#include <gui/gui.h>
#include <gui/clay_helper.h>
#include "../scene.h"

#define TAG "Header"

typedef struct {
    FuriString* version_text;
    FuriString* charge_text;
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
            .floating =
                {
                    .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_TOP, .parent = CLAY_ATTACH_POINT_CENTER_TOP},
                    .attachTo = CLAY_ATTACH_TO_ROOT,
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
        CLAY_AUTO_ID() {
            clay_fixed_image(&header_battery);
        }
    }
    return false;
}

static void header_on_alloc(Scene* scene, void* context) {
    UNUSED(context);

    View* view = scene_get_view(scene);

    view_allocate_model(view, ViewModelTypeLockFree, sizeof(HeaderViewModel));
    view_set_layout_callback(view, header_layout);
    view_set_transparent(view, true);

    with_view_model(
        view,
        HeaderViewModel * model,
        {
            const Version* version = version_get();

            model->version_text = furi_string_alloc();
            furi_string_printf(model->version_text, "%s %s", version_get_gitbranch(version), version_get_githash(version));

            // TODO: charge bar
            model->charge_text = furi_string_alloc();
            furi_string_printf(model->charge_text, "-1%%");
        },
        false);
}

const SceneCallbacks scene_header_callbacks = {
    .on_alloc = header_on_alloc,
    .on_free = NULL,
    .on_enter = NULL,
    .on_exit = NULL,
};
