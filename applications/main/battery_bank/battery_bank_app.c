#include <furi.h>
#include <gui/gui.h>
#include <gui/clay_helper.h>
#include <power/power.h>

#define TAG "BatteryBankApp"
#define POLL_INTERVAL_MS 2000

typedef struct {
    uint8_t  soc_pct;
    int16_t  ibus_ma;
    uint16_t vbus_mv;
    uint16_t time_to_empty_min;
    bool     data_valid;
    char     soc_buf[8];
    char     vbus_buf[12];
    char     ibus_buf[12];
    char     tte_buf[12];
} BatteryBankModel;

typedef struct {
    Gui*                gui;
    View*               view;
    FuriEventLoop*      event_loop;
    FuriThread*         thread;
    Power*              power;
    FuriEventLoopTimer* timer;
} BatteryBankApp;

static void battery_bank_format_time(char* buf, size_t len, uint16_t minutes) {
    if(minutes == 0xFFFF) {
        snprintf(buf, len, "--");
    } else {
        snprintf(buf, len, "%uh%02um", minutes / 60, minutes % 60);
    }
}

static bool battery_bank_layout(void* _model) {
    furi_assert(_model);
    BatteryBankModel* model = _model;

    Clay_Sizing expand = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};

    CLAY(
        CLAY_APP_ID("Root"),
        {.backgroundColor = COLOR_WHITE,
         .layout = {
             .layoutDirection = CLAY_TOP_TO_BOTTOM,
             .sizing = expand,
         }}) {

        /* Header */
        CLAY(
            CLAY_APP_ID("Header"),
            {.backgroundColor = COLOR_BLACK,
             .layout = {
                 .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(16)},
                 .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
             }}) {
            CLAY_TEXT(
                CLAY_STRING("Power Bank"),
                CLAY_TEXT_CONFIG({.fontId = FontButton, .textColor = COLOR_WHITE}));
        }

        /* SoC area */
        CLAY(
            CLAY_APP_ID("SocArea"),
            {.layout = {
                 .sizing = expand,
                 .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                 .layoutDirection = CLAY_TOP_TO_BOTTOM,
                 .childGap = 6,
             }}) {

            if(!model->data_valid) {
                CLAY_TEXT(
                    CLAY_STRING("Reading..."),
                    CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = COLOR_BLACK}));
            } else {
                CLAY(
                    CLAY_APP_ID("SocLabel"),
                    {.layout = {.childAlignment = {.x = CLAY_ALIGN_X_CENTER}}}) {
                    CLAY_TEXT(
                        clay_helper_string_from_chars(model->soc_buf),
                        CLAY_TEXT_CONFIG({.fontId = FontButton, .textColor = COLOR_BLACK}));
                }

                /* Progress bar */
                CLAY(
                    CLAY_APP_ID("BarOuter"),
                    {.backgroundColor = COLOR_BLACK,
                     .cornerRadius = CLAY_CORNER_RADIUS(3),
                     .layout = {
                         .sizing = {.width = CLAY_SIZING_FIXED(200), .height = CLAY_SIZING_FIXED(10)},
                         .padding = {1, 1, 1, 1},
                     }}) {
                    uint16_t fill = (uint16_t)(model->soc_pct * 196u / 100u);
                    CLAY(
                        CLAY_APP_ID("BarFill"),
                        {.backgroundColor = COLOR_WHITE,
                         .cornerRadius = CLAY_CORNER_RADIUS(2),
                         .layout = {
                             .sizing = {
                                 .width = CLAY_SIZING_FIXED(fill),
                                 .height = CLAY_SIZING_GROW(0),
                             },
                         }}) {}
                }
            }
        }

        /* Footer stats */
        CLAY(
            CLAY_APP_ID("Footer"),
            {.backgroundColor = COLOR_BLACK,
             .layout = {
                 .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(20)},
                 .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                 .childGap = 20,
             }}) {

            if(model->data_valid) {
                CLAY_TEXT(clay_helper_string_from_chars(model->vbus_buf), CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = COLOR_WHITE}));
                CLAY_TEXT(clay_helper_string_from_chars(model->ibus_buf), CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = COLOR_WHITE}));
                CLAY_TEXT(clay_helper_string_from_chars(model->tte_buf),  CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = COLOR_WHITE}));
            }
        }
    }

    return false;
}

static void battery_bank_poll(void* context) {
    furi_check(context);
    BatteryBankApp* app = context;

    uint8_t  soc  = 0;
    int16_t  ibus = 0;
    uint16_t vbus = 0;
    uint16_t tte  = 0xFFFF;

    bool ok = true;
    ok &= power_bq28z620_get_relative_state_of_charge(app->power, &soc);
    ok &= power_bq25792_get_ibus_ma(app->power, &ibus);
    ok &= power_bq25792_get_vbus_mv(app->power, &vbus);
    power_bq28z620_get_average_time_to_empty(app->power, &tte);

    with_view_model(
        app->view,
        BatteryBankModel * model,
        {
            if(ok) {
                model->soc_pct           = soc;
                model->ibus_ma           = ibus;
                model->vbus_mv           = vbus;
                model->time_to_empty_min = tte;
                model->data_valid        = true;
                snprintf(model->soc_buf, sizeof(model->soc_buf), "%u%%", soc);
                snprintf(model->vbus_buf, sizeof(model->vbus_buf), "%u.%02uV",
                    vbus / 1000u, (vbus % 1000u) / 10u);
                snprintf(model->ibus_buf, sizeof(model->ibus_buf), "%dmA", ibus);
                battery_bank_format_time(model->tte_buf, sizeof(model->tte_buf), tte);
            }
        },
        true);
}

static bool battery_bank_input(InputEvent* event, void* context) {
    furi_check(context);
    BatteryBankApp* app = context;

    if(event->type == InputTypePress && event->key == InputKeyBack) {
        furi_thread_signal(app->thread, FuriSignalExit, NULL);
        return true;
    }
    return false;
}

static bool battery_bank_input_touch(InputTouchEvent* event, void* context) {
    furi_check(context);
    UNUSED(event);
    UNUSED(context);
    return false;
}

static BatteryBankApp* battery_bank_app_alloc(void) {
    BatteryBankApp* app = malloc(sizeof(BatteryBankApp));

    app->gui        = furi_record_open(RECORD_GUI);
    app->power      = furi_record_open(RECORD_POWER);
    app->event_loop = furi_event_loop_alloc();
    app->thread     = furi_thread_get_current();

    app->view = view_alloc();
    view_allocate_model(app->view, ViewModelTypeLockFree, sizeof(BatteryBankModel));
    view_set_layout_callback(app->view, battery_bank_layout);
    view_set_input_callback(app->view, battery_bank_input, app);
    view_set_input_touch_callback(app->view, battery_bank_input_touch, app);
    gui_add_view(app->gui, app->view, GuiViewPriorityApplication);

    app->timer = furi_event_loop_timer_alloc(
        app->event_loop, battery_bank_poll, FuriEventLoopTimerTypePeriodic, app);
    furi_event_loop_timer_start(app->timer, furi_ms_to_ticks(POLL_INTERVAL_MS));

    battery_bank_poll(app);

    return app;
}

static void battery_bank_app_free(BatteryBankApp* app) {
    furi_event_loop_timer_free(app->timer);
    gui_remove_view(app->gui, app->view);
    view_free(app->view);
    furi_event_loop_free(app->event_loop);
    furi_record_close(RECORD_POWER);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t battery_bank_app(void* p) {
    UNUSED(p);
    BatteryBankApp* app = battery_bank_app_alloc();
    furi_event_loop_run(app->event_loop);
    battery_bank_app_free(app);
    return 0;
}
