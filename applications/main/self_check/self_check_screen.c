#include "self_check_screen.h"

#include <gui/clay_helper.h>
#include <assets.h>
#include <version.h>
#include <furi_hal_version.h>

#include <furi_bsp_expander.h>
#include <haptic/haptic.h>
#include <power/power.h>
#include <input_touch/input_touch.h>
#include <pd/pd.h>
#include <usb_mux/usb_mux.h>

#define TAG "SelfCheckScreen"

/* Positions from the mock, in pixels from the top left corner. The mock has
 * eight checks 10 pixels apart; with nine they are 9 apart to clear the footer. */
#define SELF_CHECK_SCREEN_LEFT          9
#define SELF_CHECK_SCREEN_TITLE_Y       12
#define SELF_CHECK_SCREEN_CHECKS_Y      32
#define SELF_CHECK_SCREEN_CHECKS_STEP   9
#define SELF_CHECK_SCREEN_LOGO_X        112
#define SELF_CHECK_SCREEN_LOGO_Y        12
#define SELF_CHECK_SCREEN_FOOTER_Y      117
#define SELF_CHECK_SCREEN_FOOTER_STEP   12
#define SELF_CHECK_SCREEN_HARDWARE_X    132

static const Clay_Color COLOR_LABEL = {0x69, 0x69, 0x69, 0xFF};

typedef enum {
    SelfCheckCurrentMeter,
    SelfCheckExpanderControl,
    SelfCheckExpanderMain,
    SelfCheckHaptic,
    SelfCheckPowerDelivery,
    SelfCheckCharger,
    SelfCheckGauge,
    SelfCheckTouchpad,
    SelfCheckUsbMux,
    SelfCheckCount,
} SelfCheck;

static const char* const self_check_names[SelfCheckCount] = {
    [SelfCheckCurrentMeter] = "Current Meter",
    [SelfCheckExpanderControl] = "Expander Control",
    [SelfCheckExpanderMain] = "Expander Main",
    [SelfCheckHaptic] = "Haptic",
    [SelfCheckPowerDelivery] = "Power Delivery",
    [SelfCheckCharger] = "Charger",
    [SelfCheckGauge] = "Gauge",
    [SelfCheckTouchpad] = "Touchpad",
    [SelfCheckUsbMux] = "USB Mux",
};

struct SelfCheckScreen {
    bool ok;
    FuriString* checks[SelfCheckCount];
    FuriString* serial;
    FuriString* hardware;
    FuriString* firmware;
};

// Which devices the services found when they started at MCU boot
static void self_check_screen_run(bool found[SelfCheckCount]) {
    FuriBspDevice expander = 0;
    furi_bsp_expander_is_initialized(&expander);

    Haptic* haptic_record = furi_record_open(RECORD_HAPTIC);
    HapticDevice haptic = 0;
    haptic_is_device_initialized(haptic_record, &haptic);
    furi_record_close(RECORD_HAPTIC);

    Power* power_record = furi_record_open(RECORD_POWER);
    PowerDevice power = 0;
    power_is_device_initialized(power_record, &power);
    furi_record_close(RECORD_POWER);

    InputTouch* input_touch_record = furi_record_open(RECORD_INPUT_TOUCH);
    InputTouchDevice input_touch = 0;
    input_touch_is_device_initialized(input_touch_record, &input_touch);
    furi_record_close(RECORD_INPUT_TOUCH);

    Pd* pd_record = furi_record_open(RECORD_PD);
    PdDevice pd = 0;
    pd_is_device_initialized(pd_record, &pd);
    furi_record_close(RECORD_PD);

    UsbMux* usb_mux_record = furi_record_open(RECORD_USBMUX);
    UsbMuxDevice usb_mux = 0;
    usb_mux_is_device_initialized(usb_mux_record, &usb_mux);
    furi_record_close(RECORD_USBMUX);

    found[SelfCheckCurrentMeter] = power & PowerDeviceIna219;
    found[SelfCheckExpanderControl] = expander & FuriBspDeviceExpanderControl;
    found[SelfCheckExpanderMain] = expander & FuriBspDeviceExpanderMain;
    found[SelfCheckHaptic] = haptic & HapticDeviceDrv2605l;
    found[SelfCheckPowerDelivery] = pd & PdDeviceFusb302;
    found[SelfCheckCharger] = power & PowerDeviceBq2579x;
    found[SelfCheckGauge] = power & PowerDeviceBq28z620;
    found[SelfCheckTouchpad] = input_touch & InputTouchDeviceIqs7211e;
    found[SelfCheckUsbMux] = usb_mux & UsbMuxDeviceHd3ss3220;
}

SelfCheckScreen* self_check_screen_alloc(void) {
    SelfCheckScreen* instance = malloc(sizeof(SelfCheckScreen));

    bool found[SelfCheckCount];
    self_check_screen_run(found);
    instance->ok = true;
    for(size_t i = 0; i < SelfCheckCount; i++) {
        instance->checks[i] = furi_string_alloc_printf("%s: %s", self_check_names[i], found[i] ? "OK" : "FAIL");
        if(!found[i]) instance->ok = false;
    }

    // The values follow their labels after a space
    instance->serial = furi_string_alloc_set(" ");
    FuriString* uid = furi_string_alloc();
    furi_hal_version_get_uid_str(uid);
    furi_string_cat(instance->serial, uid);
    furi_string_free(uid);

    if(furi_hal_version_is_otp_valid(FuriHalOtpBlock1)) {
        instance->hardware = furi_string_alloc_printf(
            " F%uB%uC%u", furi_hal_version_get_hw_target(), furi_hal_version_get_hw_body(), furi_hal_version_get_hw_connect());
    } else {
        instance->hardware = furi_string_alloc_set(" Unknown");
    }

    const Version* version = version_get();
    instance->firmware = furi_string_alloc_printf(" %s %s", version_get_version(version), version_get_githash(version));

    return instance;
}

void self_check_screen_free(SelfCheckScreen* instance) {
    furi_check(instance);
    for(size_t i = 0; i < SelfCheckCount; i++) {
        furi_string_free(instance->checks[i]);
    }
    furi_string_free(instance->serial);
    furi_string_free(instance->hardware);
    furi_string_free(instance->firmware);
    free(instance);
}

bool self_check_screen_is_ok(const SelfCheckScreen* instance) {
    furi_check(instance);
    return instance->ok;
}

// A row of texts placed at x, y of the parent
#define SELF_CHECK_SCREEN_AT(id, x, y) \
    CLAY(id, {.floating = {.attachTo = CLAY_ATTACH_TO_PARENT, .offset = {(x), (y)}}, .layout = {.layoutDirection = CLAY_LEFT_TO_RIGHT}})

static void self_check_screen_text(Clay_String text, uint16_t font, Clay_Color color) {
    CLAY_TEXT(text, CLAY_TEXT_CONFIG({.fontId = font, .textColor = color, .wrapMode = CLAY_TEXT_WRAP_NONE}));
}

static void self_check_screen_field(Clay_ElementId id, float x, float y, const char* label, FuriString* value) {
    SELF_CHECK_SCREEN_AT(id, x, y) {
        self_check_screen_text(clay_helper_string_from_chars(label), FontBody, COLOR_LABEL);
        self_check_screen_text(clay_helper_string_from(value), FontBody, COLOR_WHITE);
    }
}

void self_check_screen_layout(const SelfCheckScreen* instance) {
    furi_check(instance);

    CLAY(
        CLAY_APP_ID("Screen"),
        {
            .backgroundColor = COLOR_BLACK,
            .layout = {.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)}},
        }) {
        SELF_CHECK_SCREEN_AT(CLAY_APP_ID("Title"), SELF_CHECK_SCREEN_LEFT, SELF_CHECK_SCREEN_TITLE_Y) {
            self_check_screen_text(CLAY_STRING("Self Check"), FontBig, COLOR_WHITE);
        }

        for(size_t i = 0; i < SelfCheckCount; i++) {
            SELF_CHECK_SCREEN_AT(
                CLAY_IDI(TAG "Check", i), SELF_CHECK_SCREEN_LEFT, SELF_CHECK_SCREEN_CHECKS_Y + i * SELF_CHECK_SCREEN_CHECKS_STEP) {
                self_check_screen_text(clay_helper_string_from(instance->checks[i]), FontBody, COLOR_WHITE);
            }
        }

        SELF_CHECK_SCREEN_AT(CLAY_APP_ID("Logo"), SELF_CHECK_SCREEN_LOGO_X, SELF_CHECK_SCREEN_LOGO_Y) {
            clay_fixed_image(CLAY_ID_LOCAL("Image"), &self_check_logo);
        }

        self_check_screen_field(
            CLAY_APP_ID("Serial"), SELF_CHECK_SCREEN_LEFT, SELF_CHECK_SCREEN_FOOTER_Y, "Serial:", instance->serial);
        self_check_screen_field(
            CLAY_APP_ID("Hardware"), SELF_CHECK_SCREEN_HARDWARE_X, SELF_CHECK_SCREEN_FOOTER_Y, "Hardware:", instance->hardware);
        self_check_screen_field(
            CLAY_APP_ID("Firmware"),
            SELF_CHECK_SCREEN_LEFT,
            SELF_CHECK_SCREEN_FOOTER_Y + SELF_CHECK_SCREEN_FOOTER_STEP,
            "MCU Firmware:",
            instance->firmware);
    }
}
