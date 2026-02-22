#include <furi.h>
#include <gui/gui.h>
#include <gui/clay_helper.h>
#include <haptic/haptic.h>
#include "haptic_effect_name.h"
#include <drivers/drv2605l/drv2605l_effect.h>

#define TAG "HapticTest"

typedef struct {
    uint32_t effect_index;
    int32_t play_time_ms;
    FuriString* effect_name;
    FuriString* play_time_ms_str;
} HapticTestModel;

typedef struct {
    Gui* gui;
    View* view;
    FuriEventLoop* event_loop;
    FuriThread* thread;
    Haptic* haptic;
} HapticTest;

static bool haptic_test_layout(void* _model) {
    furi_assert(_model);
    HapticTestModel* model = (HapticTestModel*)_model;
    UNUSED(model);
    Clay_Sizing layout_expand = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
    Clay_LayoutConfig layout_row = {
        .sizing = {.height = CLAY_SIZING_FIXED(14), .width = CLAY_SIZING_GROW(0)},
        .childGap = 8,
        .childAlignment =
            {
                .y = CLAY_ALIGN_Y_CENTER,
                .x = CLAY_ALIGN_X_CENTER,
            },
    };

    CLAY(
        CLAY_APP_ID("OuterContainer"),
        {.backgroundColor = COLOR_WHITE,
         .layout = {
             .layoutDirection = CLAY_TOP_TO_BOTTOM,
             .sizing = layout_expand,
             .padding = {4, 4, 4, 3},
             .childGap = 4,
         }}) {
        CLAY(
            CLAY_APP_ID("Header"),
            {
                .layout =
                    {
                        .sizing = {.height = CLAY_SIZING_FIXED(14), .width = CLAY_SIZING_GROW(0)},
                        .childGap = 8,
                        .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                    },
            }) {
            CLAY_AUTO_ID({.layout = {.padding = {8, 8, 4, 4}}}) {
                CLAY_TEXT(CLAY_STRING("Haptic Test"), CLAY_TEXT_CONFIG({.fontId = FontButton, .textColor = COLOR_BLACK}));
            }
        }
        CLAY(
            CLAY_APP_ID("MainContent"),
            {
                .border = {.color = COLOR_BLACK, .width = {.top = 1, .left = 1, .right = 1, .bottom = 1}},
                .cornerRadius = CLAY_CORNER_RADIUS(4),
                .clip = {.vertical = true},
                .layout =
                    {
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        .childGap = 8,
                        .padding = {6, 6, 6, 6},
                        .sizing = layout_expand,
                        .childAlignment = {.y = CLAY_ALIGN_Y_CENTER},
                    },
            }) {
            CLAY_AUTO_ID({.layout = layout_row}) {
                CLAY_AUTO_ID({.layout = {.padding = {8, 8, 4, 4}}}) {
                    CLAY_TEXT(CLAY_STRING("Effect"), CLAY_TEXT_CONFIG({.fontId = FontButton, .textColor = COLOR_BLACK}));
                }
                CLAY_TEXT(clay_helper_string_from(model->effect_name), CLAY_TEXT_CONFIG({.fontId = FontButton, .textColor = COLOR_BLACK}));
            }
            CLAY_AUTO_ID({.layout = layout_row}) {
                CLAY_TEXT(CLAY_STRING("Play time"), CLAY_TEXT_CONFIG({.fontId = FontButton, .textColor = COLOR_BLACK}));
                CLAY_TEXT(clay_helper_string_from(model->play_time_ms_str), CLAY_TEXT_CONFIG({.fontId = FontButton, .textColor = COLOR_BLACK}));
            }
        }
    }

    return false;
}

// static Drv2605lEffect haptic_test_get_play_effect(HapticTest* instance) {
//     Drv2605lEffect effect = 0;
//     with_view_model(instance->view, HapticTestModel * model, { effect = model->effect_index; }, false);
//     return effect;
// }

static bool haptic_test_input(InputEvent* event, void* context) {
    furi_check(context);
    HapticTest* instance = context;
    bool consumed = false;

    if(event->key == InputKeyBack) {
        if(event->type == InputTypePress) {
            furi_thread_signal(instance->thread, FuriSignalExit, NULL);
            consumed = true;
        }
    }

    if(event->type == InputTypePress) {
        if(event->key == InputKeyUp) {
            with_view_model(
                instance->view,
                HapticTestModel * model,
                {
                    model->effect_index = (model->effect_index + 1) % Drv2605lEffectCountMax;
                    furi_string_set(model->effect_name, haptic_effect_names[model->effect_index]);
                    haptic_play_effect(instance->haptic, model->effect_index, model->play_time_ms);
                },
                true);
            consumed = true;
        }

        if(event->key == InputKeyDown) {
            with_view_model(
                instance->view,
                HapticTestModel * model,
                {
                    model->effect_index = (model->effect_index - 1 + Drv2605lEffectCountMax) % Drv2605lEffectCountMax;
                    furi_string_set(model->effect_name, haptic_effect_names[model->effect_index]);
                    haptic_play_effect(instance->haptic, model->effect_index, model->play_time_ms);
                },
                true);
            consumed = true;
        }

        if(event->key == InputKeyOk) {
            with_view_model(instance->view, HapticTestModel * model, { haptic_play_effect(instance->haptic, model->effect_index, model->play_time_ms); }, true);
        }

        if(event->key == InputKeyLeft) {
            with_view_model(
                instance->view,
                HapticTestModel * model,
                {
                    model->play_time_ms = model->play_time_ms > 2 ? model->play_time_ms - 1 : 0;
                    furi_string_printf(model->play_time_ms_str, "%ld ms", model->play_time_ms);
                    haptic_play_effect(instance->haptic, model->effect_index, model->play_time_ms);
                },
                true);
            consumed = true;
        }
        if(event->key == InputKeyRight) {
            with_view_model(
                instance->view,
                HapticTestModel * model,
                {
                    model->play_time_ms = model->play_time_ms < 2999 ? model->play_time_ms + 1 : 3000;
                    model->play_time_ms = model->play_time_ms <= 1 ? 2 : model->play_time_ms;
                    furi_string_printf(model->play_time_ms_str, "%ld ms", model->play_time_ms);
                    haptic_play_effect(instance->haptic, model->effect_index, model->play_time_ms);
                },
                true);
            consumed = true;
        }
    }

    if(event->type == InputTypeRepeat) {
        if(event->key == InputKeyUp) {
            with_view_model(
                instance->view,
                HapticTestModel * model,
                {
                    model->effect_index = (model->effect_index + 1) % Drv2605lEffectCountMax;
                    furi_string_set(model->effect_name, haptic_effect_names[model->effect_index]);
                },
                true);
            consumed = true;
        }

        if(event->key == InputKeyDown) {
            with_view_model(
                instance->view,
                HapticTestModel * model,
                {
                    model->effect_index = (model->effect_index - 1 + Drv2605lEffectCountMax) % Drv2605lEffectCountMax;
                    furi_string_set(model->effect_name, haptic_effect_names[model->effect_index]);
                },
                true);
            consumed = true;
        }
        if(event->key == InputKeyLeft) {
            with_view_model(
                instance->view,
                HapticTestModel * model,
                {
                    model->play_time_ms = model->play_time_ms > 9 ? model->play_time_ms - 10 : 0;
                    furi_string_printf(model->play_time_ms_str, "%ld ms", model->play_time_ms);
                },
                true);
            consumed = true;
        }
        if(event->key == InputKeyRight) {
            with_view_model(
                instance->view,
                HapticTestModel * model,
                {
                    model->play_time_ms = model->play_time_ms < 2991 ? model->play_time_ms + 10 : 3000;
                    furi_string_printf(model->play_time_ms_str, "%ld ms", model->play_time_ms);
                },
                true);
            consumed = true;
        }
    }

    return consumed;
}

static bool haptic_test_input_touch(InputTouchEvent* event, void* context) {
    furi_check(context);
    HapticTest* instance = context;
    UNUSED(event);
    UNUSED(instance);

    return false;
}

static HapticTest* haptic_test_alloc(void) {
    HapticTest* instance = malloc(sizeof(HapticTest));
    instance->gui = furi_record_open(RECORD_GUI);
    instance->event_loop = furi_event_loop_alloc();
    instance->thread = furi_thread_get_current();
    instance->haptic = furi_record_open(RECORD_HAPTIC);
    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLockFree, sizeof(HapticTestModel));
    with_view_model(
        instance->view,
        HapticTestModel * model,
        {
            model->effect_index = 0;
            model->effect_name = furi_string_alloc();
            furi_string_set(model->effect_name, haptic_effect_names[model->effect_index]);
            model->play_time_ms = 0;
            model->play_time_ms_str = furi_string_alloc();
            furi_string_printf(model->play_time_ms_str, "%ld ms", model->play_time_ms);
        },
        false);

    view_set_layout_callback(instance->view, haptic_test_layout);
    view_set_input_callback(instance->view, haptic_test_input, instance);
    view_set_input_touch_callback(instance->view, haptic_test_input_touch, instance);
    gui_add_view(instance->gui, instance->view, GuiViewPriorityApplication);
    return instance;
}

static void haptic_test_free(HapticTest* instance) {
    gui_remove_view(instance->gui, instance->view);
    furi_record_close(RECORD_GUI);
    with_view_model(
        instance->view,
        HapticTestModel * model,
        {
            furi_string_free(model->effect_name);
            furi_string_free(model->play_time_ms_str);
        },
        false);
    furi_record_close(RECORD_HAPTIC);
    view_free(instance->view);
    furi_event_loop_free(instance->event_loop);
    free(instance);
}

int32_t haptic_test_app(void* p) {
    UNUSED(p);
    HapticTest* instance = haptic_test_alloc();
    furi_event_loop_run(instance->event_loop);
    haptic_test_free(instance);
    return 0;
}
