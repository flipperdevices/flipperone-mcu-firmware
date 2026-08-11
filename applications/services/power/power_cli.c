#include "power_cli.h"

#include <cli/args.h>
#include <toolbox/strint.h>
#include <cli/cli_ansi.h>
#include <cli/cli_command.h>
#include <furi_hal.h>
#include <toolbox/property.h>

#include <power/power.h>
#include "power_show_cli.h"
#include "power_consumption_cli.h"

typedef struct {
    bool is_charging;
    bool is_full_charged;
    bool charge_enabled;
    uint8_t charge;

    int16_t current_battery;
    int16_t current_usb;

    float voltage_battery;
    float voltage_usb;

    float temperature_charger;
    float temperature_battery;

    uint16_t charge_ilim_usb;
    uint16_t charge_ilim_battery;
    uint16_t charge_level_limit;

    struct {
        Bq2579xChargerStatusReg charger_status;
        Bq2579xFaultStatusReg charger_fault;
    } debug;
} PowerInfo;

typedef struct {
    const char* name;
    const char* arg_spec;
    const char* description;
    bool (*execute)(PipeSide*, FuriString*);
} PowerCmd;

static bool power_cli_off(PipeSide* pipe, FuriString* args) {
    UNUSED(pipe);
    UNUSED(args);
    Power* power = furi_record_open(RECORD_POWER);
    printf("Disconnect USB for shutdown\r\n");

    furi_delay_ms(100);
    bool success = false;
    do {
        power_bq2579x_usb_is_connected(power, &success);
        furi_delay_ms(1000);
    } while(success);

    success = power_bq2579x_set_power_switch(power, Bq2579xPowerShutdown);

    furi_record_close(RECORD_POWER);
    return success;
}

static bool power_cli_ship_mode(PipeSide* pipe, FuriString* args) {
    UNUSED(pipe);
    UNUSED(args);
    Power* power = furi_record_open(RECORD_POWER);
    printf("Disconnect USB for Ship Mode\r\n");

    furi_delay_ms(100);
    bool success = false;
    do {
        power_bq2579x_usb_is_connected(power, &success);
        furi_delay_ms(1000);
    } while(success);

    success = power_bq2579x_set_power_switch(power, Bq2579xPowerShipMode);

    furi_record_close(RECORD_POWER);
    return success;
}

static bool power_cli_reboot(PipeSide* pipe, FuriString* args) {
    UNUSED(pipe);
    UNUSED(args);
    Power* power = furi_record_open(RECORD_POWER);

    furi_delay_ms(100);
    bool success = false;
    do {
        success = power_bq2579x_set_power_switch(power, Bq2579xPowerReset);
        furi_delay_ms(1000);
    } while(!success);

    furi_record_close(RECORD_POWER);
    return success;
}

static void power_cli_print_property(const char* key, const char* value, bool last, void* context) {
    UNUSED(last);
    UNUSED(context);
    printf("%-30s: %s\r\n", key, value);
}

static void power_get_info(Power* power, PowerInfo* info) {
    furi_check(power);
    furi_check(info);

    power_bq2579x_get_charger_status(power, &info->debug.charger_status);
    power_bq2579x_get_charger_fault(power, &info->debug.charger_fault);

    info->is_charging = info->debug.charger_status.stat1.chg_stat == Bq2579xChargerStatus1ChargeNot ? false : true;
    info->is_full_charged = info->debug.charger_status.stat1.chg_stat == Bq2579xChargerStatus1ChargeTermination ? true : false;
    power_bq2579x_charge_is_enabled(power, &info->charge_enabled);

    power_bq28z620_get_relative_state_of_charge(power, &info->charge);
    power_bq28z620_get_voltage(power, &info->voltage_battery);
    power_bq28z620_get_current(power, &info->current_battery);
    power_bq28z620_get_temperature(power, &info->temperature_battery);

    info->charge_level_limit = 0xFF;
    //power_bq28z620_get_state_of_health(power, &info->charge_level_limit);

    uint16_t vsys_mv = 0;
    power_bq2579x_get_vbus_mv(power, &vsys_mv);
    info->voltage_usb = vsys_mv / 1000.0f;
    int16_t ibus_ma = 0;
    power_bq2579x_get_ibus_ma(power, &ibus_ma);
    info->current_usb = ibus_ma;

    power_bq2579x_get_ico_current_limit_ma(power, (uint16_t*)&info->charge_ilim_usb);
    power_bq2579x_get_charge_current_limit_ma(power, (uint16_t*)&info->charge_ilim_battery);

    power_bq2579x_get_charger_temperature(power, &info->temperature_charger);
}

static bool power_cli_info(PipeSide* pipe, FuriString* args) {
    UNUSED(args);

    Power* power = furi_record_open(RECORD_POWER);
    PowerInfo info;
    power_get_info(power, &info);
    furi_record_close(RECORD_POWER);

    FuriString* value = furi_string_alloc();
    FuriString* key = furi_string_alloc();

    PropertyValueContext prop_ctx = {
        .key = key,
        .value = value,
        .out = power_cli_print_property,
        .sep = '.',
        .last = false,
        .context = pipe,
    };
    if(info.is_charging) {
        property_value_out(&prop_ctx, "%s", 1, "state", (info.is_full_charged) ? "charged" : "charging");
    } else {
        property_value_out(&prop_ctx, "%s", 1, "state", "discharging");
    }

    property_value_out(&prop_ctx, "%u%%", 2, "BAT", "level", info.charge);
    property_value_out(&prop_ctx, "%.0f mV", 2, "BAT", "voltage", info.voltage_battery * 1000.0f);
    property_value_out(&prop_ctx, "%d mA", 2, "BAT", "current", info.current_battery);
    property_value_out(&prop_ctx, "%.1fC", 2, "BAT", "NTC", info.temperature_battery);

    property_value_out(&prop_ctx, "%.0f mV", 2, "USB", "voltage", info.voltage_usb * 1000.0f);
    property_value_out(&prop_ctx, "%u mA", 2, "USB", "current", info.current_usb);
    property_value_out(&prop_ctx, "%u mA", 2, "USB", "current_limit", info.charge_ilim_usb);

    property_value_out(&prop_ctx, "%u", 2, "charger", "enabled", info.charge_enabled);
    property_value_out(&prop_ctx, "%lu%%", 2, "charger", "level_limit", info.charge_level_limit);
    property_value_out(&prop_ctx, "%u mA", 2, "charger", "current_limit", info.charge_ilim_battery);
    property_value_out(&prop_ctx, "%.1fC", 2, "charger", "temperature", info.temperature_charger);

#if POWER_CLI_DEBUG == 1
    power_cli_info_print_debug(&prop_ctx, &info);
#endif

    furi_string_free(value);
    furi_string_free(key);
    return true;
}

static const PowerCmd power_cmds[] = {
    {"info", "", "Show power info", power_cli_info},
    {"show", "", "Show power status", power_show_cli},
    {"consumption", "", "Show power consumption", power_consumption_cli},
    {"off", "", "Power off the device, WARNING: Powers on only when connected via USB.", power_cli_off},
    {"ship", "", "Enter ship mode", power_cli_ship_mode},
    {"reboot", "", "Reboot the device", power_cli_reboot},
};

static void power_cli_print_usage(void) {
    printf("Usage:\r\npower <cmd>\r\nCmd list:\r\n");
    for(size_t i = 0; i < COUNT_OF(power_cmds); i++) {
        const PowerCmd* c = &power_cmds[i];
        printf("\t%s %s - %s\r\n", c->name, c->arg_spec, c->description);
    }
}

void power_cli(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(context);
    FuriString* cmd = furi_string_alloc();
    bool handled = false;

    if(args_read_string_and_trim(args, cmd)) {
        const char* cmd_str = furi_string_get_cstr(cmd);
        for(size_t i = 0; i < COUNT_OF(power_cmds); i++) {
            const PowerCmd* c = &power_cmds[i];
            if(strcmp(cmd_str, c->name) == 0) {
                if(!c->execute(pipe, args)) {
                    printf("usage: power %s %s\r\n", c->name, c->arg_spec);
                }
                handled = true;
                break;
            }
        }
    }

    if(!handled) {
        power_cli_print_usage();
    }

    furi_string_free(cmd);
}
