#include <furi.h>
#include <gui/modules/menu.h>
#include <gui/gui.h>
#include "power_meter_i.h"

#define TAG "PowerMeterGui"

typedef struct {
    Gui* gui;
    View* view;
    Menu* menu;
    FuriEventLoop* event_loop;
    FuriThread* thread;

    PowerMeterState state;
} PowerMeterGui;

enum {
    MenuItemInState,
    MenuItemInPower,
    MenuItemBatteryPercent,
    MenuItemBatteryPower,
    MenuItemError,
};

static const struct {
    const char* text;
    bool connected;
    bool pd_ok;
} in_state_values[] = {
    {"Not connected", false, false},
    {"No PD", true, false},
    {"PD OK", true, true},
};
static const uint8_t in_power_values[] = {0, 5, 10, 20, 30};
static const uint8_t battery_pct_values[] = {1, 50, 100};
static const uint8_t battery_power_values[] = {0, 1, 3, 5, 7, 10, 20, 30};

static void power_meter_menu_callback(MenuItem* item, size_t item_id, void* context) {
    furi_check(context);
    PowerMeterGui* instance = context;

    if(item == NULL) {
        FURI_LOG_I(TAG, "Exit");
        furi_thread_signal(instance->thread, FuriSignalExit, NULL);
    }
}

static void power_meter_menu_in_state_callback(MenuItem* item, size_t item_id, void* context) {
    furi_check(context);
    PowerMeterGui* instance = context;
    menu_item_selector_set_value(instance->menu, item, item_id, in_state_values[item_id].text);

    instance->state.in_connected = in_state_values[item_id].connected;
    instance->state.in_pd_valid = in_state_values[item_id].pd_ok;
}

static void power_meter_menu_in_power_callback(MenuItem* item, size_t item_id, void* context) {
    furi_check(context);
    PowerMeterGui* instance = context;
    FuriString* value_text = furi_string_alloc();
    furi_string_printf(value_text, "%u W", in_power_values[item_id]);
    menu_item_selector_set_value(instance->menu, item, item_id, furi_string_get_cstr(value_text));
    furi_string_free(value_text);

    instance->state.in_power_mw = in_power_values[item_id] * 1000;
}

static void power_meter_menu_battery_power_callback(MenuItem* item, size_t item_id, void* context) {
    furi_check(context);
    PowerMeterGui* instance = context;
    FuriString* value_text = furi_string_alloc();
    furi_string_printf(value_text, "%u W", battery_power_values[item_id]);
    menu_item_selector_set_value(instance->menu, item, item_id, furi_string_get_cstr(value_text));
    furi_string_free(value_text);

    instance->state.power_consumption_mw = battery_power_values[item_id] * 1000;
}

static void power_meter_menu_battery_percent_callback(MenuItem* item, size_t item_id, void* context) {
    furi_check(context);
    PowerMeterGui* instance = context;
    FuriString* value_text = furi_string_alloc();
    furi_string_printf(value_text, "%u %%", battery_pct_values[item_id]);
    menu_item_selector_set_value(instance->menu, item, item_id, furi_string_get_cstr(value_text));
    furi_string_free(value_text);

    instance->state.battery_percent = battery_pct_values[item_id];
}

static void power_meter_menu_error_callback(MenuItem* item, size_t item_id, void* context) {
    furi_check(context);
    PowerMeterGui* instance = context;
    menu_item_selector_set_value(instance->menu, item, item_id, item_id == 0 ? "No" : "Yes");

    instance->state.error = (item_id == 1);
}

static PowerMeterGui* power_meter_test_alloc(void) {
    PowerMeterGui* instance = malloc(sizeof(PowerMeterGui));
    instance->event_loop = furi_event_loop_alloc();
    instance->thread = furi_thread_get_current();

    instance->gui = furi_record_open(RECORD_GUI);
    instance->view = view_alloc();
    gui_add_view(instance->gui, instance->view, GuiViewPriorityApplication);

    instance->menu = menu_alloc(instance->view);
    menu_set_callback(instance->menu, power_meter_menu_callback, instance);
    menu_set_title(instance->menu, "PowerMeter demo");

    MenuItem* item = menu_add_item(instance->menu, "In state", MenuItemInState, MenuItemSubTypeSelector);
    menu_item_selector_configure(instance->menu, item, sizeof(in_state_values) / sizeof(in_state_values[0]), power_meter_menu_in_state_callback);
    power_meter_menu_in_state_callback(item, 0, instance);

    item = menu_add_item(instance->menu, "In power", MenuItemInPower, MenuItemSubTypeSelector);
    menu_item_selector_configure(instance->menu, item, sizeof(in_power_values) / sizeof(in_power_values[0]), power_meter_menu_in_power_callback);
    power_meter_menu_in_power_callback(item, 0, instance);

    item = menu_add_item(instance->menu, "Battery %", MenuItemBatteryPercent, MenuItemSubTypeSelector);
    menu_item_selector_configure(instance->menu, item, sizeof(battery_pct_values) / sizeof(battery_pct_values[0]), power_meter_menu_battery_percent_callback);
    power_meter_menu_battery_percent_callback(item, 0, instance);

    item = menu_add_item(instance->menu, "Power consumption", MenuItemBatteryPower, MenuItemSubTypeSelector);
    menu_item_selector_configure(instance->menu, item, sizeof(battery_power_values) / sizeof(battery_power_values[0]), power_meter_menu_battery_power_callback);
    power_meter_menu_battery_power_callback(item, 0, instance);

    item = menu_add_item(instance->menu, "Error", MenuItemError, MenuItemSubTypeSelector);
    menu_item_selector_configure(instance->menu, item, 2, power_meter_menu_error_callback);
    power_meter_menu_error_callback(item, 0, instance);

    return instance;
}

static void power_meter_test_free(PowerMeterGui* instance) {
    menu_free(instance->menu);

    gui_remove_view(instance->gui, instance->view);
    view_free(instance->view);

    furi_record_close(RECORD_GUI);
    furi_event_loop_free(instance->event_loop);
    free(instance);
}

int32_t power_meter_test_app(void* p) {
    UNUSED(p);

    PowerMeterGui* instance = power_meter_test_alloc();

    // Start actual PowerMeter service
    FuriThread* srv_thread = furi_thread_alloc_ex("PowerMeter", 4096, power_meter_srv, (void*)instance);
    furi_thread_start(srv_thread);

    furi_event_loop_run(instance->event_loop);
    power_meter_test_free(instance);

    furi_thread_signal(srv_thread, FuriSignalExit, NULL);
    furi_thread_join(srv_thread);
    furi_thread_free(srv_thread);
    return 0;
}

// TODO: change to actual power API calls
void power_meter_test_get_state(PowerMeterState* state, void* context) {
    furi_check(state);
    furi_check(context);
    PowerMeterGui* instance = context;

    memcpy(state, &instance->state, sizeof(PowerMeterState));
}
