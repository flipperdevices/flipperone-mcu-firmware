#include <assets.h>
#include <gui/gui.h>
#include <gui/clay_helper.h>
#include <gui/modules/elements.h>
#include <furi_hal_power.h>
#include <furi_hal_flash.h>
#include "../scene.h"
#include "../desktop_i.h"

#define TAG "DebugApps"

#define DEBUG_MENU_ID(x) CLAY_SIDI(CLAY_STRING("DebugMenu"), x)

typedef struct {
    uint32_t selected_index;
} DebugMenuViewModel;

static bool debug_menu_layout(void* _model) {
    DebugMenuViewModel* model = _model;
    furi_assert(model);

    CLAY(
        CLAY_APP_ID("Container"),
        {
            .backgroundColor = COLOR_WHITE,
            .layout =
                {
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                    .childGap = 4,
                    .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                },
            .clip = {.vertical = true, .childOffset = Clay_GetScrollOffset()},
        }) {
        for(uint32_t i = 0; i < FLIPPER_APPS_COUNT; i++) {
            bool selected = (i == model->selected_index);
            CLAY(
                DEBUG_MENU_ID(i),
                {
                    .layout =
                        {
                            .sizing = {.width = CLAY_SIZING_FIXED(120), .height = CLAY_SIZING_FIXED(13)},
                            .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                        },
                    .backgroundColor = selected ? COLOR_BLACK : COLOR_WHITE,
                    .cornerRadius = CLAY_CORNER_RADIUS(2),
                }) {
                CLAY_TEXT(
                    clay_helper_string_from_chars(FLIPPER_APPS[i].name),
                    CLAY_TEXT_CONFIG({
                        .fontId = FontBody,
                        .textColor = selected ? COLOR_WHITE : COLOR_BLACK,
                    }));
            }
        }
    }

    elements_softkey_button_element(0, "Rollback", false, false);
    elements_softkey_button_element(4, "DFU", false, false);

    return false;
}

static bool debug_menu_input(InputEvent* event, void* context) {
    bool consumed = false;
    Scene* scene = context;
    Desktop* desktop = scene_get_data(scene);
    View* view = scene_get_view(scene);

    if(event->type == InputTypePress && event->key == InputKeyOk) {
        uint32_t selected_index;
        with_view_model(view, DebugMenuViewModel * model, { selected_index = model->selected_index; }, false);
        desktop_start_app(&FLIPPER_APPS[selected_index]);
        consumed = true;
    } else if(event->type == InputTypePress && event->key == InputKeyBack) {
        scene_exit(scene, desktop);
        consumed = true;
    } else if(event->type == InputTypePress && event->key == InputKey5) {
        furi_hal_power_enter_bootsel();
        consumed = true;
    } else if(event->type == InputTypePress && event->key == InputKey1) {
        furi_hal_flash_rollback();
        furi_hal_power_reset();
        consumed = true;
    } else if((event->type == InputTypePress) || (event->type == InputTypeRepeat)) {
        switch(event->key) {
        case InputKeyDown:
            with_view_model(view, DebugMenuViewModel * model, { model->selected_index = (model->selected_index + 1) % FLIPPER_APPS_COUNT; }, true);
            consumed = true;
            break;
        case InputKeyUp:
            with_view_model(
                view, DebugMenuViewModel * model, { model->selected_index = (model->selected_index - 1 + FLIPPER_APPS_COUNT) % FLIPPER_APPS_COUNT; }, true);
            consumed = true;
            break;
        default:
            break;
        }
    }

    return consumed;
}

static void debug_menu_on_enter(Scene* scene, void* context) {
    View* view = scene_get_view(scene);
    scene_set_data(scene, context);

    view_allocate_model(view, ViewModelTypeLockFree, sizeof(DebugMenuViewModel));

    view_set_layout_callback(view, debug_menu_layout);
    view_set_input_callback(view, debug_menu_input, scene);

    with_view_model(view, DebugMenuViewModel * model, { model->selected_index = 0; }, false);
}

static void debug_menu_on_exit(Scene* scene, void* context) {
    UNUSED(context);
    View* view = scene_get_view(scene);
    view_set_layout_callback(view, NULL);
    view_set_post_layout_callback(view, NULL);
    view_set_input_callback(view, NULL, NULL);
    view_free_model(view);
    scene_set_data(scene, NULL);
}

const SceneCallbacks scene_debug_menu_callbacks = {
    .on_alloc = NULL,
    .on_enter = debug_menu_on_enter,
    .on_exit = debug_menu_on_exit,
    .on_event = NULL,
};
