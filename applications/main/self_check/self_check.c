#include "self_test.h"
#include <gui/gui.h>
#include <gui/clay_helper.h>

#include <furi_bsp_expander.h>
#include <haptic/haptic.h>
#include <power/power.h>
#include <input_touch/input_touch.h>
#include <drivers/display/display_jd9853_qspi.h>
//#include <fusb302/fusb302.h>

#define TAG "SelfCheck"

#define KEYPAD_TEST_BUTTON_WIDTH CLAY_SIZING_FIXED(40)

typedef struct {
    FuriString* status_str;
} SelfCheckModel;

typedef struct {
    Gui* gui;
    View* view;
    FuriEventLoop* event_loop;
    FuriThread* thread;
    FuriString* status_str;
} SelfCheck;

static void keypad_test_app_create_keypad_button(Clay_String text, bool inverted) {
    CLAY_AUTO_ID({
        .border = {.color = COLOR_BLACK, .width = {.top = 1, .left = 1, .right = 1, .bottom = 1}},
        .layout =
            {
                .padding = {8, 8, 4, 4},
                .sizing = {.width = KEYPAD_TEST_BUTTON_WIDTH},
                .childAlignment = {.x = CLAY_ALIGN_X_CENTER},
            },
        .backgroundColor = inverted ? COLOR_WHITE : COLOR_BLACK,
        .cornerRadius = CLAY_CORNER_RADIUS(4),
    }) {
        CLAY_TEXT(text, CLAY_TEXT_CONFIG({.fontId = FontButton, .textColor = inverted ? COLOR_BLACK : COLOR_WHITE}));
    }
}

static bool self_check_layout(void* _model) {
    furi_assert(_model);
    SelfCheckModel* model = (SelfCheckModel*)_model;

    CLAY(
        CLAY_APP_ID("OuterContainer"),
        {.backgroundColor = COLOR_WHITE,
         .layout = {
             .layoutDirection = CLAY_TOP_TO_BOTTOM,
             .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
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
                CLAY_TEXT(CLAY_STRING("Self Check"), CLAY_TEXT_CONFIG({.fontId = FontButton, .textColor = COLOR_BLACK}));
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
                        .layoutDirection = CLAY_LEFT_TO_RIGHT,
                        .childGap = 8,
                        .padding = {6, 6, 6, 6},
                        .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                        .childAlignment = {.y = CLAY_ALIGN_Y_CENTER},
                    },
            }) {
            CLAY_AUTO_ID({.layout = {.padding = {8, 8, 4, 4}}}) {
                CLAY_TEXT(clay_helper_string_from(model->status_str), CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = COLOR_BLACK}));
            }
        }
    }

    CLAY(
        CLAY_APP_ID("Footer"),
        {
            .layout =
                {
                    .layoutDirection = CLAY_LEFT_TO_RIGHT,
                    .sizing = {.height = CLAY_SIZING_FIXED(28), .width = CLAY_SIZING_GROW(0)},
                    .padding = {0, 6, 4, 6},
                    .childAlignment = {.y = CLAY_ALIGN_Y_CENTER},
                },
        }) {
        /* spacer grows to push button to the right */
        CLAY_AUTO_ID({.layout = {.sizing = {.width = CLAY_SIZING_GROW(1)}}}) {
        }
        CLAY_AUTO_ID({.layout = {.sizing = {.width = KEYPAD_TEST_BUTTON_WIDTH}}}) {
            keypad_test_app_create_keypad_button(CLAY_STRING("Ok"), false);
        }
    }

    return false;
}

static bool self_check_input(InputEvent* event, void* context) {
    furi_check(context);
    SelfCheck* instance = context;
    bool consumed = false;

    if(event->type == InputTypePress) {
        if(event->key == InputKeyOk) {
            furi_thread_signal(instance->thread, FuriSignalExit, NULL);
            consumed = true;
        }
    }
    return consumed;
}

static bool self_check_input_touch(InputTouchEvent* event, void* context) {
    furi_check(context);
    SelfCheck* instance = context;
    UNUSED(event);
    UNUSED(instance);

    return false;
}

bool self_check_process(FuriString* status_str) {
    bool check_ok = true;
    bool all_ok = true;

    // check expander
    FuriBspDevice expander_device;
    check_ok = furi_bsp_expander_is_initialized(&expander_device);
    if(all_ok) {
        all_ok = check_ok;
    }

    // check haptic
    Haptic* haptic = furi_record_open(RECORD_HAPTIC);
    HapticDevice haptic_device;
    check_ok = haptic_is_device_initialized(haptic, &haptic_device);
    furi_record_close(RECORD_HAPTIC);
    if(all_ok) {
        all_ok = check_ok;
    }

    // check power
    Power* power = furi_record_open(RECORD_POWER);
    PowerDevice power_device;
    check_ok = power_is_device_initialized(power, &power_device);
    furi_record_close(RECORD_POWER);
    if(all_ok) {
        all_ok = check_ok;
    }

    // check display
    // todo: tps62868x is not needed, it will be removed in the next version

    // check input touch
    InputTouch* input_touch = furi_record_open(RECORD_INPUT_TOUCH);
    InputTouchDevice input_touch_device;
    check_ok = input_touch_is_device_initialized(input_touch, &input_touch_device);
    furi_record_close(RECORD_INPUT_TOUCH);
    if(all_ok) {
        all_ok = check_ok;
    }

    // check usb
    // todo: add usb check when fusb302 is ready

    // show result
    if(status_str) {
        furi_string_set(status_str, "Checking\n\n");
        furi_string_cat_printf(status_str, "Expander control: %s\n", (expander_device & FuriBspDeviceExpanderControl) ? "OK" : "FAIL");
        furi_string_cat_printf(status_str, "Expander main: %s\n", (expander_device & FuriBspDeviceExpanderMain) ? "OK" : "FAIL");
        furi_string_cat_printf(status_str, "Haptic: %s\n", (haptic_device & HapticDeviceDrv2605l) ? "OK" : "FAIL");
        furi_string_cat_printf(status_str, "Power INA219: %s\n", (power_device & PowerDeviceIna219) ? "OK" : "FAIL");
        furi_string_cat_printf(status_str, "Power BQ25792: %s\n", (power_device & PowerDeviceBq25792) ? "OK" : "FAIL");
        furi_string_cat_printf(status_str, "Power BQ28z620: %s\n", (power_device & PowerDeviceBq28z620) ? "OK" : "FAIL");
        furi_string_cat_printf(status_str, "Input Touch: %s\n", (input_touch_device & InputTouchDeviceIqs7211e) ? "OK" : "FAIL");
    }

    return all_ok;
}

static SelfCheck* self_check_alloc(void) {
    SelfCheck* instance = malloc(sizeof(SelfCheck));
    instance->gui = furi_record_open(RECORD_GUI);
    instance->event_loop = furi_event_loop_alloc();
    instance->thread = furi_thread_get_current();
    instance->view = view_alloc();

    instance->status_str = furi_string_alloc();
    self_check_process(instance->status_str);

    view_allocate_model(instance->view, ViewModelTypeLockFree, sizeof(SelfCheckModel));
    with_view_model(instance->view, SelfCheckModel * model, { model->status_str = instance->status_str; }, false);

    view_set_layout_callback(instance->view, self_check_layout);
    view_set_input_callback(instance->view, self_check_input, instance);
    view_set_input_touch_callback(instance->view, self_check_input_touch, instance);
    gui_add_view(instance->gui, instance->view, GuiViewPriorityApplication);
    return instance;
}

static void self_check_free(SelfCheck* instance) {
    gui_remove_view(instance->gui, instance->view);
    furi_record_close(RECORD_GUI);
    furi_string_free(instance->status_str);
    view_free(instance->view);
    furi_event_loop_free(instance->event_loop);
    free(instance);
}

int32_t self_check_app(void* p) {
    UNUSED(p);
    SelfCheck* instance = self_check_alloc();
    furi_event_loop_run(instance->event_loop);
    self_check_free(instance);
    return 0;
}
