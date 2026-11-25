#include <assets.h>
#include <gui/gui.h>
#include <gui/clay_helper.h>
#include <gui/modules/elements.h>
#include "../scene.h"
#include <power/power.h>
#include "../desktop_i.h"

#define TAG "Header"

/*
 * Two looks: the compact bar of the settings and debug scenes (the small
 * battery with its percentage), and the home screen bar after the mock-up,
 * laid out in the mock-up's 256 px coordinates and right-aligned on the
 * 258 px display (HEADER_X_OFFSET). While charging, a black plate carries
 * the percentage in the big font and the dark battery with the bolt, and a
 * light gray plate to its left carries the input power and the time to full.
 */
#define HEADER_COMPACT_HEIGHT 11
#define HEADER_HOME_HEIGHT    22
#define HEADER_X_OFFSET       2

#define HEADER_BATTERY_X 212
#define HEADER_BATTERY_Y 3

/* Charging plates (spec "Battery UI: Charging"): the light plate and its two
 * lines sit further left when the watts have a fraction. Text positions are
 * the box's top-left; the glyphs start one pixel higher. */
#define HEADER_CHARGING_BG_X 153
#define HEADER_CHARGING_GRAY_BG_X_WHOLE 66
#define HEADER_CHARGING_GRAY_BG_X_FRACTION 58
#define HEADER_CHARGING_LINE1_X_WHOLE 75
#define HEADER_CHARGING_LINE2_X_WHOLE 83
#define HEADER_CHARGING_LINE1_X_FRACTION 67
#define HEADER_CHARGING_LINE2_X_FRACTION 72
#define HEADER_CHARGING_LINE1_Y 3
#define HEADER_CHARGING_LINE2_Y 14
/* "64.43" in the big font, then the design's own percent sign: the sign's
 * right edge at 210, the digits one pixel before it (their box extends 3 px
 * past the last glyph). */
#define HEADER_CHARGING_PERCENT_SIGN_X 199
#define HEADER_CHARGING_PERCENT_SIGN_Y 6
#define HEADER_CHARGING_PERCENT_X 200 /* right edge of the digits' box */
#define HEADER_CHARGING_PERCENT_Y 6

/* Slide-in when charging starts: both plates with their text come in from
 * 84 px to the right over 200 ms, in these whole-pixel steps at 60 fps. The
 * battery stays put and the percentage shows once the plates have arrived. */
#define HEADER_SLIDE_FRAME_MS 16
static const uint8_t header_slide_shift[] = {84, 77, 71, 63, 56, 50, 42, 35, 29, 21, 14, 8, 0};

typedef enum {
    HeaderPowerStateIdle,
    HeaderPowerStateCharging,
} HeaderPowerState;

typedef struct {
    Desktop* desktop;
    ElementsBattery battery;
    HeaderPowerState power_state;
    /* Tick the current charging session started at, for the slide-in. */
    uint32_t charging_since_ms;
    /* The input watts print without a fraction ("18W"). */
    bool watts_whole;
    /* State of charge: the gauge's whole percent and the finer capacity ratio. */
    uint8_t soc;
    float percent;
    FuriString* charge_text; /* "64%" (compact) */
    FuriString* percent_text; /* "64.43%" (charging) */
    FuriString* line1; /* "Charging: 12V 2.93W" */
    FuriString* line2; /* "Full in: 4h 33m" */
} HeaderViewModel;

static Clay_FloatingElementConfig header_float_at(int16_t x, int16_t y, Clay_FloatingAttachPointType element_point) {
    return (Clay_FloatingElementConfig){
        .offset = {.x = x + HEADER_X_OFFSET, .y = y},
        .attachPoints = {.element = element_point, .parent = CLAY_ATTACH_POINT_LEFT_TOP},
        .attachTo = CLAY_ATTACH_TO_ROOT,
    };
}

static void header_layout_compact(HeaderViewModel* model) {
    CLAY(
        CLAY_APP_ID("Container"),
        {
            .backgroundColor = COLOR_WHITE,
            .layout =
                {
                    .layoutDirection = CLAY_LEFT_TO_RIGHT,
                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(HEADER_COMPACT_HEIGHT)},
                    .childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER},
                    .padding = {.left = 3, .right = 2, .top = 0, .bottom = 0},
                },
        }) {
        CLAY(
            CLAY_APP_ID("ChargeWrapper"),
            {
                .layout =
                    {
                        .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                        .childAlignment = {.x = CLAY_ALIGN_X_RIGHT, .y = CLAY_ALIGN_Y_CENTER},
                        .padding = {.left = 0, .right = 0, .top = 2, .bottom = 0},
                    },
            }) {
            CLAY_TEXT(clay_helper_string_from(model->charge_text), CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = COLOR_BLACK}));
        }
        CLAY(
            CLAY_APP_ID("BatteryIcon"),
            {
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
            CLAY(
                CLAY_APP_ID("BatteryChargeIndicator"),
                {
                    .backgroundColor = {0, 0, 0, 0xFF},
                    .layout =
                        {
                            .sizing =
                                {
                                    .width = CLAY_SIZING_PERCENT((float)model->soc / 100.f),
                                    .height = CLAY_SIZING_GROW(0),
                                },
                        },
                }){};
        }
    }
}

static void header_layout_home(HeaderViewModel* model) {
    const bool charging = (model->power_state == HeaderPowerStateCharging);

    /* Slide-in progress: frame index at 60 fps since charging began. */
    int32_t shift = 0;
    bool sliding = false;
    if(charging) {
        uint32_t frame = (furi_get_tick() - model->charging_since_ms) / HEADER_SLIDE_FRAME_MS;
        if(frame < COUNT_OF(header_slide_shift) - 1) {
            shift = header_slide_shift[frame];
            sliding = true;
        }
    }

    CLAY(
        CLAY_APP_ID("Container"),
        {
            .backgroundColor = COLOR_WHITE,
            .layout =
                {
                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(HEADER_HOME_HEIGHT)},
                },
        }) {
        if(charging) {
            const int16_t gray_x = model->watts_whole ? HEADER_CHARGING_GRAY_BG_X_WHOLE : HEADER_CHARGING_GRAY_BG_X_FRACTION;
            const int16_t line1_x = model->watts_whole ? HEADER_CHARGING_LINE1_X_WHOLE : HEADER_CHARGING_LINE1_X_FRACTION;
            const int16_t line2_x = model->watts_whole ? HEADER_CHARGING_LINE2_X_WHOLE : HEADER_CHARGING_LINE2_X_FRACTION;

            CLAY(
                CLAY_APP_ID("ChargingGrayBackdrop"),
                {
                    .floating = header_float_at(gray_x + shift, 0, CLAY_ATTACH_POINT_LEFT_TOP),
                }) {
                clay_fixed_image(CLAY_ID_LOCAL("Image"), &header_battery_gray_bg);
            }
            CLAY(
                CLAY_APP_ID("ChargingBackdrop"),
                {
                    .floating = header_float_at(HEADER_CHARGING_BG_X + shift, 0, CLAY_ATTACH_POINT_LEFT_TOP),
                }) {
                clay_fixed_image(CLAY_ID_LOCAL("Image"), &header_battery_black_bg);
            }
            CLAY(
                CLAY_APP_ID("ChargingLine1"),
                {
                    .floating = header_float_at(line1_x + shift, HEADER_CHARGING_LINE1_Y, CLAY_ATTACH_POINT_LEFT_TOP),
                }) {
                CLAY_TEXT(clay_helper_string_from(model->line1), CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = COLOR_BLACK, .wrapMode = CLAY_TEXT_WRAP_NONE}));
            }
            CLAY(
                CLAY_APP_ID("ChargingLine2"),
                {
                    .floating = header_float_at(line2_x + shift, HEADER_CHARGING_LINE2_Y, CLAY_ATTACH_POINT_LEFT_TOP),
                }) {
                CLAY_TEXT(clay_helper_string_from(model->line2), CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = COLOR_BLACK, .wrapMode = CLAY_TEXT_WRAP_NONE}));
            }
            if(sliding) {
                clay_request_redraw(CLAY_APP_ID("ChargingSlide"), HEADER_SLIDE_FRAME_MS);
            } else {
                CLAY(
                    CLAY_APP_ID("ChargingPercent"),
                    {
                        .floating = header_float_at(HEADER_CHARGING_PERCENT_X, HEADER_CHARGING_PERCENT_Y, CLAY_ATTACH_POINT_RIGHT_TOP),
                    }) {
                    CLAY_TEXT(
                        clay_helper_string_from(model->percent_text),
                        CLAY_TEXT_CONFIG({.fontId = FontBig, .textColor = COLOR_WHITE, .wrapMode = CLAY_TEXT_WRAP_NONE}));
                }
                CLAY(
                    CLAY_APP_ID("ChargingPercentSign"),
                    {
                        .floating = header_float_at(HEADER_CHARGING_PERCENT_SIGN_X, HEADER_CHARGING_PERCENT_SIGN_Y, CLAY_ATTACH_POINT_LEFT_TOP),
                    }) {
                    clay_fixed_image(CLAY_ID_LOCAL("Image"), &header_percent_big);
                }
            }
        }
        CLAY(
            CLAY_APP_ID("BatteryWrapper"),
            {
                .floating = header_float_at(HEADER_BATTERY_X, HEADER_BATTERY_Y, CLAY_ATTACH_POINT_LEFT_TOP),
            }) {
            elements_battery_element(CLAY_APP_ID("Battery"), &model->battery, model->percent, charging ? ElementsBatteryLookDark : ElementsBatteryLookLight);
        }
    }
}

static bool header_layout(void* _model) {
    furi_assert(_model);
    HeaderViewModel* model = _model;

    if(desktop_is_home_shown(model->desktop)) {
        header_layout_home(model);
    } else {
        header_layout_compact(model);
    }
    return false;
}

static void header_update_battery_state(Scene* scene) {
    View* view = scene_get_view(scene);

    uint8_t soc = 0;
    uint16_t remaining_mah = 0;
    uint16_t full_mah = 0;
    uint16_t time_to_full_min = 0;
    uint16_t vbus_mv = 0;
    int16_t ibus_ma = 0;
    Bq2579xChargerStatusReg status = {0};

    Power* power = furi_record_open(RECORD_POWER);
    bool soc_ok = power_bq28z620_get_relative_state_of_charge(power, &soc);
    bool capacity_ok = power_bq28z620_get_remaining_capacity(power, &remaining_mah) && power_bq28z620_get_full_charge_capacity(power, &full_mah) && full_mah > 0;
    bool time_ok = power_bq28z620_get_average_time_to_full(power, &time_to_full_min);
    bool status_ok = power_bq2579x_get_charger_status(power, &status);
    bool input_ok = power_bq2579x_get_vbus_mv(power, &vbus_mv) && power_bq2579x_get_ibus_ma(power, &ibus_ma);
    furi_record_close(RECORD_POWER);

    if(!soc_ok) return;
    if(soc > 100) soc = 100;
    float percent = capacity_ok ? (float)remaining_mah * 100.f / (float)full_mah : (float)soc;
    if(percent > 100.f) percent = 100.f;

    bool charging = false;
    if(status_ok && status.stat0.vbus_present_stat) {
        switch(status.stat1.chg_stat) {
        case Bq2579xChargerStatus1ChargeTrickle:
        case Bq2579xChargerStatus1ChargePre:
        case Bq2579xChargerStatus1ChargeFast:
        case Bq2579xChargerStatus1ChargeTaper:
        case Bq2579xChargerStatus1ChargeTopOff:
            charging = true;
            break;
        default:
            break;
        }
    }

    with_view_model(
        view,
        HeaderViewModel * model,
        {
            model->soc = soc;
            model->percent = percent;
            if(charging && model->power_state != HeaderPowerStateCharging) {
                model->charging_since_ms = furi_get_tick();
            }
            model->power_state = charging ? HeaderPowerStateCharging : HeaderPowerStateIdle;
            furi_string_printf(model->charge_text, "%u%%", soc);
            furi_string_printf(model->percent_text, "%.2f", (double)percent);

            if(charging) {
                if(input_ok) {
                    float input_w = (float)vbus_mv / 1000.f * (float)ibus_ma / 1000.f;
                    unsigned centiwatts = (unsigned)(input_w * 100.f + 0.5f);
                    model->watts_whole = (centiwatts % 100 == 0);
                    if(model->watts_whole) {
                        furi_string_printf(model->line1, "Charging: %uV %uW", (unsigned)((vbus_mv + 500) / 1000), centiwatts / 100);
                    } else {
                        furi_string_printf(model->line1, "Charging: %uV %u.%02uW", (unsigned)((vbus_mv + 500) / 1000), centiwatts / 100, centiwatts % 100);
                    }
                } else {
                    model->watts_whole = true;
                    furi_string_set(model->line1, "Charging");
                }
                /* 65535 is the gauge's "never" (the current is too small to tell). */
                if(time_ok && time_to_full_min != 0xFFFF) {
                    furi_string_printf(model->line2, "Full in: %uh %um", time_to_full_min / 60, time_to_full_min % 60);
                } else {
                    furi_string_set(model->line2, "Full in: --");
                }
            }
        },
        true);
}

static void header_on_alloc(Scene* scene, void* context) {
    View* view = scene_get_view(scene);

    view_allocate_model(view, ViewModelTypeLockFree, sizeof(HeaderViewModel));
    view_set_layout_callback(view, header_layout);

    with_view_model(
        view,
        HeaderViewModel * model,
        {
            model->desktop = context;
            elements_battery_init(&model->battery);
            model->charge_text = furi_string_alloc();
            model->percent_text = furi_string_alloc();
            model->line1 = furi_string_alloc();
            model->line2 = furi_string_alloc();
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
        header_update_battery_state(scene);
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
