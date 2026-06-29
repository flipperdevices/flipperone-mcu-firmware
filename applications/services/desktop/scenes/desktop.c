#include <assets.h>
#include <gui/gui.h>
#include <gui/clay_helper.h>
#include "../scene.h"
#include "../elements.h"
#include "../desktop_i.h"

#define TAG "Desktop"

typedef struct {
    bool help_pressed;
    bool power_pressed;
    bool settings_pressed;
    Desktop* desktop;
} DesktopViewModel;

static bool desktop_layout(void* _model) {
    DesktopViewModel* model = _model;
    furi_assert(model);

    CLAY(
        CLAY_APP_ID("Container"),
        {
            .backgroundColor = COLOR_WHITE,
            .layout =
                {
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                    .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                },
            .clip = {.vertical = true, .childOffset = Clay_GetScrollOffset()},
        }) {
        // for(uint32_t i = 0; i < FLIPPER_APPS_COUNT; i++) {
        //     bool selected = (i == model->selected_index);
        //     CLAY(
        //         DESKTOP_MENU_ID(i),
        //         {
        //             .layout =
        //                 {
        //                     .sizing = {.width = CLAY_SIZING_FIXED(120), .height = CLAY_SIZING_FIXED(13)},
        //                     .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
        //                 },
        //             .backgroundColor = selected ? COLOR_BLACK : COLOR_WHITE,
        //             .cornerRadius = CLAY_CORNER_RADIUS(2),
        //         }) {
        //         CLAY_TEXT(
        //             clay_helper_string_from_chars(FLIPPER_APPS[i].name),
        //             CLAY_TEXT_CONFIG({
        //                 .fontId = FontBody,
        //                 .textColor = selected ? COLOR_WHITE : COLOR_BLACK,
        //             }));
        //     }
        // }

        CLAY_AUTO_ID({
            .layout =
                {
                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                    .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                },
        }) {
            clay_fixed_image(&face_sleep);
        }

        CLAY_AUTO_ID({
            .layout =
                {
                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)},
                    .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                    .childGap = 4,
                },
        }) {
            if(model->help_pressed) {
                elements_softkey_button_element("Help", SoftkeyButtonStatePressed);
            } else {
                if(false) {
                    elements_softkey_button_element("Help", SoftkeyButtonStateActive);
                } else {
                    elements_softkey_button_element("Help", SoftkeyButtonStateInactive);
                }
            }

            if(model->power_pressed) {
                elements_softkey_button_element("Power", SoftkeyButtonStatePressed);
            } else {
                if(desktop_get_power_menu_state(model->desktop)) {
                    elements_softkey_button_element("Power", SoftkeyButtonStateActive);
                } else {
                    elements_softkey_button_element("Power", SoftkeyButtonStateInactive);
                }
            }

            if(model->settings_pressed) {
                elements_softkey_button_element("Settings", SoftkeyButtonStatePressed);
            } else {
                if(false) {
                    elements_softkey_button_element("Settings", SoftkeyButtonStateActive);
                } else {
                    elements_softkey_button_element("Settings", SoftkeyButtonStateInactive);
                }
            }
        }
    }

    return false;
}

static bool desktop_input(InputEvent* event, void* context) {
    bool consumed = false;
    Desktop* desktop = scene_get_data(context);
    View* view = scene_get_view(context);

    if(event->type == InputTypePress) {
        switch(event->key) {
        case InputKey2:
            with_view_model(view, DesktopViewModel * model, { model->help_pressed = true; }, true);
            consumed = true;
            break;
        case InputKey3:
            with_view_model(view, DesktopViewModel * model, { model->power_pressed = true; }, true);
            if(!desktop_get_power_menu_state(desktop)) {
                desktop_show_power_menu(desktop);
            } else {
                desktop_hide_power_menu(desktop);
            }
            consumed = true;
            break;
        case InputKey4:
            with_view_model(view, DesktopViewModel * model, { model->settings_pressed = true; }, true);
            consumed = true;
            break;
        default:
            break;
        }
    } else if(event->type == InputTypeRelease) {
        switch(event->key) {
        case InputKey2:
            with_view_model(view, DesktopViewModel * model, { model->help_pressed = false; }, true);
            consumed = true;
            break;
        case InputKey3:
            with_view_model(view, DesktopViewModel * model, { model->power_pressed = false; }, true);
            consumed = true;
            break;
        case InputKey4:
            with_view_model(view, DesktopViewModel * model, { model->settings_pressed = false; }, true);
            consumed = true;
            break;
        default:
            break;
        }
    }

    return consumed;
}

static void desktop_on_alloc(Scene* scene, void* context) {
    View* view = scene_get_view(scene);
    scene_set_data(scene, context);

    view_allocate_model(view, ViewModelTypeLockFree, sizeof(DesktopViewModel));
    with_view_model(view, DesktopViewModel * model, { model->desktop = context; }, false);

    view_set_layout_callback(view, desktop_layout);
    view_set_input_callback(view, desktop_input, scene);
}

const SceneCallbacks scene_desktop_callbacks = {
    .on_alloc = desktop_on_alloc,
    .on_free = NULL,
    .on_enter = NULL,
    .on_exit = NULL,
};
