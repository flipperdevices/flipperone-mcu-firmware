#include "power_cli.h"

#include <cli/args.h>
#include <toolbox/strint.h>
#include <cli/cli_ansi.h>
#include <cli/cli_command.h>
#include <furi_hal.h>

#include <power/power.h>
#include "power_show_cli.h"
#include "power_consumption_cli.h"

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
        power_bq25792_usb_is_connected(power, &success);
        furi_delay_ms(1000);
    } while(success);

    success = power_bq25792_set_power_switch(power, Bq25792PowerShutdown);

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
        power_bq25792_usb_is_connected(power, &success);
        furi_delay_ms(1000);
    } while(success);

    success = power_bq25792_set_power_switch(power, Bq25792PowerShipMode);

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
        success = power_bq25792_set_power_switch(power, Bq25792PowerReset);
        furi_delay_ms(1000);
    } while(!success);

    furi_record_close(RECORD_POWER);
    return success;
}

static const PowerCmd power_cmds[] = {
    {"show", "", "Show power status", power_show_cli},
    {"consumption", "", "Show power consumption", power_consumption_cli},
    {"off", "", "Power off the device, WARNING: Powers on only when connected via USB.", power_cli_off},
    {"ship", "", "Enter ship mode", power_cli_ship_mode},
    {"reboot", "", "Reboot the device", power_cli_reboot},

    //{"list", "", "List available I2C buses", power_cli_list},
    // {"search", "<bus_name>", "Search for devices on the specified I2C bus", power_cli_search},
    // {"write", "<bus_name> <device_hex> <reg_hex> <data_hex>", "Write data to the specified I2C device", power_cli_write},
    // {"read", "<bus_name> <device_hex> <reg_hex> <length>", "Read data from the specified I2C device", power_cli_read},
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
