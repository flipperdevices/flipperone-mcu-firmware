#include <gui/gui.h>
#include <gui/clay_helper.h>

#include <furi_bsp_expander.h>
#include <haptic/haptic.h>
#include <power/power.h>
#include <input_touch/input_touch.h>
#include <drivers/display/display_jd9853_qspi.h>
#include <pd/pd.h>
#include <usb_mux/usb_mux.h>
#include <assets.h>
#include <desktop/desktop.h>

#define TAG "SelfCheck"

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

static bool self_check_layout(void* _model) {
    furi_assert(_model);
    SelfCheckModel* model = (SelfCheckModel*)_model;

    CLAY(
        CLAY_APP_ID("OuterContainer"),
        {.backgroundColor = COLOR_WHITE,
         .layout = {
             .layoutDirection = CLAY_TOP_TO_BOTTOM,
             .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
             .padding = {4, 4, 4, 4},
             .childGap = 4,
         }}) {
        CLAY(
            CLAY_APP_ID("MainContent"),
            {
                .clip = {.vertical = true},
                .layout =
                    {
                        .layoutDirection = CLAY_LEFT_TO_RIGHT,
                        .childGap = 8,
                        .padding = {.left = 6, .right = 6, .top = 2, .bottom = 2},
                        .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                    },
            }) {
            // image wrapper with floating position
            CLAY_AUTO_ID({
                .layout =
                    {
                        .sizing = {.height = CLAY_SIZING_FIT(0), .width = CLAY_SIZING_FIT(0)},
                        .padding = {.left = 0, .right = 3, .top = 3, .bottom = 0},
                    },
                .floating =
                    {
                        .attachPoints = {.element = CLAY_ATTACH_POINT_RIGHT_TOP, .parent = CLAY_ATTACH_POINT_RIGHT_TOP},
                        .attachTo = CLAY_ATTACH_TO_PARENT,
                    },
            }) {
                clay_fixed_image(&logo_head);
            }

            // text
            CLAY_AUTO_ID() {
                CLAY_TEXT(clay_helper_string_from(model->status_str), CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = COLOR_BLACK}));
            }
        }
    }

    return false;
}

static bool self_check_input(InputEvent* event, void* context) {
    furi_check(context);
    SelfCheck* instance = context;
    bool consumed = false;

    if(event->type == InputTypePress) {
        if(event->key == InputKeyOk || event->key == InputKeyBack) {
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

static bool self_check_process(FuriString* status_str) {
    bool check_ok = true;
    bool all_ok = true;

    // check expander
    FuriBspDevice expander_device = 0;
    check_ok = furi_bsp_expander_is_initialized(&expander_device);
    if(all_ok) {
        all_ok = check_ok;
    }

    // check haptic
    Haptic* haptic = furi_record_open(RECORD_HAPTIC);
    HapticDevice haptic_device = 0;
    check_ok = haptic_is_device_initialized(haptic, &haptic_device);
    furi_record_close(RECORD_HAPTIC);
    if(all_ok) {
        all_ok = check_ok;
    }

    // check power
    Power* power = furi_record_open(RECORD_POWER);
    PowerDevice power_device = 0;
    check_ok = power_is_device_initialized(power, &power_device);
    furi_record_close(RECORD_POWER);
    if(all_ok) {
        all_ok = check_ok;
    }

    // check display
    // TODO: tps62868x is not needed, it will be removed in the next version

    // check input touch
    InputTouch* input_touch = furi_record_open(RECORD_INPUT_TOUCH);
    InputTouchDevice input_touch_device = 0;
    check_ok = input_touch_is_device_initialized(input_touch, &input_touch_device);
    furi_record_close(RECORD_INPUT_TOUCH);
    if(all_ok) {
        all_ok = check_ok;
    }

    // check pd
    Pd* pd = furi_record_open(RECORD_PD);
    PdDevice pd_device = 0;
    check_ok = pd_is_device_initialized(pd, &pd_device);
    furi_record_close(RECORD_PD);
    if(all_ok) {
        all_ok = check_ok;
    }

    // check usb mux
    UsbMux* usb_mux = furi_record_open(RECORD_USBMUX);
    UsbMuxDevice usb_mux_device = 0;
    check_ok = usb_mux_is_device_initialized(usb_mux, &usb_mux_device);
    furi_record_close(RECORD_USBMUX);
    if(all_ok) {
        all_ok = check_ok;
    }

    // show result
    if(status_str) {
        // TODO: get real cpu info
        furi_string_printf(status_str, "CPU: Dual Cortex-M33 @ 150MHz\n");
        furi_string_cat_printf(status_str, "Memory total: %dK\n", memmgr_get_total_heap() / 1024);
        furi_string_cat_printf(status_str, "Memory free: %dK\n\n", memmgr_get_free_heap() / 1024);
        furi_string_cat_printf(status_str, "Current meter: %s\n", (power_device & PowerDeviceIna219) ? "ok" : "NOT FOUND");
        furi_string_cat_printf(status_str, "Expander control: %s\n", (expander_device & FuriBspDeviceExpanderControl) ? "ok" : "NOT FOUND");
        furi_string_cat_printf(status_str, "Expander main: %s\n", (expander_device & FuriBspDeviceExpanderMain) ? "ok" : "NOT FOUND");
        furi_string_cat_printf(status_str, "Haptic: %s\n", (haptic_device & HapticDeviceDrv2605l) ? "ok" : "NOT FOUND");
        furi_string_cat_printf(status_str, "PD: %s\n", (pd_device & PdDeviceFusb302) ? "ok" : "NOT FOUND");
        furi_string_cat_printf(status_str, "Charger: %s\n", (power_device & PowerDeviceBq2579x) ? "ok" : "NOT FOUND");
        furi_string_cat_printf(status_str, "Gauge: %s\n", (power_device & PowerDeviceBq28z620) ? "ok" : "NOT FOUND");
        furi_string_cat_printf(status_str, "Touchpad: %s\n", (input_touch_device & InputTouchDeviceIqs7211e) ? "ok" : "NOT FOUND");
        furi_string_cat_printf(status_str, "USB Mux: %s\n", (usb_mux_device & UsbMuxDeviceHd3ss3220) ? "ok" : "NOT FOUND");
        furi_string_cat_printf(status_str, "\nPress DEL to enter setup, OK or BACK to exit");
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
    if(!self_check_process(instance->status_str)) {
        Haptic* haptic = furi_record_open(RECORD_HAPTIC);
        haptic_play_effect(haptic, Drv2605lEffectDoubleClick_100, 0);
        furi_record_close(RECORD_HAPTIC);
    }

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

static void self_check_app_main(void) {
    SelfCheck* instance = self_check_alloc();
    furi_event_loop_run(instance->event_loop);
    self_check_free(instance);
}

static void self_check_app_autorun(void) {
    FURI_LOG_I(TAG, "Starting self check autorun");

    // Occupy the desktop app slot right away: no other app can be started
    // via desktop while we are running (and desktop can stop us on demand).
    desktop_register_app("self_check", furi_thread_get_current());

    if(!self_check_process(NULL)) {
        self_check_app_main();
    }

    desktop_unregister_app("self_check");
}

int32_t self_check_app(void* p) {
    if(p && strcmp((char*)p, "autorun") == 0) {
        self_check_app_autorun();
    } else {
        self_check_app_main();
    }

    return 0;
}
