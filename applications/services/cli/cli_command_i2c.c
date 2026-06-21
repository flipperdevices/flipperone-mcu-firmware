#include "cli_command_i2c.h"

#include <cli/args.h>
#include <toolbox/strint.h>
#include <cli/cli_ansi.h>
#include <cli/cli_command.h>
#include <furi_hal.h>
#include <furi_hal_i2c_types.h>
#include <furi_hal_i2c_config.h>

#define I2C_BUS_CONTROL "control"
#define I2C_BUS_MAIN    "main"

typedef struct {
    const char* name;
    const char* arg_spec;
    const char* description;
    bool (*execute)(PipeSide*, FuriString*);
} I2CCmd;

typedef struct {
    const char* name;
    const FuriHalI2cBusHandle* handle;
} I2CBusInfo;

static const I2CBusInfo i2c_buses[] = {
    {I2C_BUS_CONTROL, &furi_hal_i2c_handle_control},
    {I2C_BUS_MAIN, &furi_hal_i2c_handle_main},
};

static const I2CBusInfo* cli_command_i2c_bus_by_name(FuriString* name) {
    for(size_t i = 0; i < COUNT_OF(i2c_buses); i++) {
        char name_buffer[16];
        snprintf(name_buffer, sizeof(name_buffer), "%zu", i); // The buffer name can be specified by its index in the array.
        if(furi_string_cmp(name, i2c_buses[i].name) == 0 || furi_string_cmp(name, name_buffer) == 0) {
            return &i2c_buses[i];
        }
    }
    return NULL;
}

static void cli_command_i2c_scan_bus(const FuriHalI2cBusHandle* handle, const char* bus_name) {
    furi_check(handle);
    furi_hal_i2c_acquire(handle);

    printf("Scanning %s bus (%s):\r\n", bus_name, furi_hal_i2c_bus_name(handle));
    printf("     0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\r\n");
    printf("    -----------------------------------------------\r\n");

    for(uint8_t addr = 0; addr < 128; addr++) {
        if(addr % 16 == 0) {
            printf("%02x | ", addr);
        }

        // Perform a 1-byte dummy read from the probe address. If a slave
        // acknowledges this address, the function returns the number of bytes
        // transferred. If the address byte is ignored, the function returns
        // -1.

        // Skip over any reserved addresses.
        bool ret = furi_hal_i2c_device_ready(handle, addr, FURI_HAL_I2C_TIMEOUT_US);
        printf(ret ? "@" : ".");
        printf(addr % 16 == 15 ? "\r\n" : "  ");
    }
    furi_hal_i2c_release(handle);
}

static bool cli_command_i2c_list(PipeSide* pipe, FuriString* args) {
    UNUSED(pipe);
    UNUSED(args);
    printf("Available I2C buses:\r\n");
    for(size_t i = 0; i < COUNT_OF(i2c_buses); i++) {
        printf(" %zu - %s\r\n", i, i2c_buses[i].name);
    }
    return true;
}

static bool cli_command_i2c_search(PipeSide* pipe, FuriString* args) {
    UNUSED(pipe);

    const I2CBusInfo* bus_info = cli_command_i2c_bus_by_name(args);
    if(!bus_info) {
        printf(ANSI_FG_RED "Unknown I2C bus:" ANSI_RESET " %s\r\n", furi_string_get_cstr(args));
        return false;
    }

    cli_command_i2c_scan_bus(bus_info->handle, bus_info->name);
    return true;
}

static bool cli_command_i2c_write(PipeSide* pipe, FuriString* args) {
    UNUSED(pipe);
    bool result = false;
    FuriString* arg_bus_name = furi_string_alloc();
    FuriString* data_str = furi_string_alloc();
    FuriString* data_write = furi_string_alloc();
    uint8_t* data = NULL;

    do {
        uint8_t device_address;
        uint8_t device_register;
        size_t data_length;
        const I2CBusInfo* bus_info;

        if(!furi_string_size(args)) {
            break;
        }

        args_read_string_and_trim(args, arg_bus_name);
        bus_info = cli_command_i2c_bus_by_name(arg_bus_name);

        if(!bus_info) {
            printf(ANSI_FG_RED "Unknown I2C bus:" ANSI_RESET " %s\r\n", furi_string_get_cstr(arg_bus_name));
            break;
        }

        const char* args_cstr = furi_string_get_cstr(args);
        StrintParseError parse_err = StrintParseNoError;
        parse_err |= strint_to_uint8(args_cstr, &args_cstr, &device_address, 16);
        parse_err |= strint_to_uint8(args_cstr, &args_cstr, &device_register, 16);
        if(parse_err || (device_address > 0x7F) || ((device_address & 0x78) == 0) || ((device_address & 0x78) == 0x78)) {
            printf(ANSI_FG_RED "Invalid device address or register:" ANSI_RESET " %s\r\n", furi_string_get_cstr(args));
            break;
        }

        furi_string_set(data_str, args_cstr);
        furi_string_trim(data_str);
        args_read_string_and_trim(data_str, data_write);

        printf("Data to write: '%s'\r\n", furi_string_get_cstr(data_write));

        data_length = furi_string_size(data_write) / 2;
        if(data_length == 0) {
            printf(ANSI_FG_RED "No data provided to write\r\n" ANSI_RESET);
            break;
        }

        data = malloc(data_length + 1); // +1 for device register
        data[0] = device_register;
        if(!args_read_hex_bytes(data_write, data + 1, data_length)) {
            printf(ANSI_FG_RED "Failed" ANSI_RESET " to read hex data\r\n");
            break;
        }

        furi_hal_i2c_acquire(bus_info->handle);
        int success = furi_hal_i2c_master_tx_blocking(bus_info->handle, device_address, data, data_length + 1, FURI_HAL_I2C_TIMEOUT_US);
        furi_hal_i2c_release(bus_info->handle);

        if(success >= PICO_OK) {
            printf(ANSI_FG_GREEN "Written" ANSI_RESET " to device 0x%02X register 0x%02X:", device_address, device_register);
            for(size_t i = 1; i < data_length + 1; i++) {
                printf(" %02X", data[i]);
            }
            printf("\r\n");
        } else {
            printf(ANSI_FG_RED "Failed" ANSI_RESET " to write to bus %s device 0x%02X register 0x%02X\r\n", bus_info->name, device_address, device_register);
        }

        result = true;
    } while(0);

    if(data) {
        free(data);
    }
    furi_string_free(arg_bus_name);
    furi_string_free(data_str);
    furi_string_free(data_write);
    return result;
}

static bool cli_command_i2c_read(PipeSide* pipe, FuriString* args) {
    UNUSED(pipe);

    FuriString* arg_bus_name = furi_string_alloc();
    bool result = false;

    do {
        uint8_t device_address;
        uint8_t device_register;
        uint8_t length;
        const I2CBusInfo* bus_info;

        if(!furi_string_size(args)) {
            break;
        }

        args_read_string_and_trim(args, arg_bus_name);
        bus_info = cli_command_i2c_bus_by_name(arg_bus_name);

        if(!bus_info) {
            printf(ANSI_FG_RED "Unknown I2C bus:" ANSI_RESET " %s\r\n", furi_string_get_cstr(arg_bus_name));
            break;
        }

        const char* args_cstr = furi_string_get_cstr(args);
        StrintParseError parse_err = StrintParseNoError;
        parse_err |= strint_to_uint8(args_cstr, &args_cstr, &device_address, 16);
        parse_err |= strint_to_uint8(args_cstr, &args_cstr, &device_register, 16);
        parse_err |= strint_to_uint8(args_cstr, &args_cstr, &length, 10);
        if(parse_err || (device_address > 0x7F) || ((device_address & 0x78) == 0) || ((device_address & 0x78) == 0x78)) {
            printf(ANSI_FG_RED "Invalid device address or register:" ANSI_RESET " %s\r\n", furi_string_get_cstr(args));
            break;
        }

        {
            uint8_t* buffer = malloc(length);

            furi_hal_i2c_acquire(bus_info->handle);
            int success = furi_hal_i2c_master_trx_blocking(bus_info->handle, device_address, &device_register, 1, buffer, length, FURI_HAL_I2C_TIMEOUT_US);
            furi_hal_i2c_release(bus_info->handle);

            if(success >= PICO_OK) {
                printf(ANSI_FG_GREEN "Read" ANSI_RESET " from device 0x%02X register 0x%02X:", device_address, device_register);
                for(size_t i = 0; i < length; i++) {
                    printf(" %02X", buffer[i]);
                }
                printf("\r\n");
            } else {
                printf(
                    ANSI_FG_RED "Failed" ANSI_RESET " to read bus %s from device 0x%02X register 0x%02X\r\n", bus_info->name, device_address, device_register);
            }

            free(buffer);
        }

        result = true;
    } while(0);

    furi_string_free(arg_bus_name);

    return result;
}

static const I2CCmd i2c_cmds[] = {
    {"list", "", "List available I2C buses", cli_command_i2c_list},
    {"search", "<bus_name>", "Search for devices on the specified I2C bus", cli_command_i2c_search},
    {"write", "<bus_name> <device_hex> <reg_hex> <data_hex>", "Write data to the specified I2C device", cli_command_i2c_write},
    {"read", "<bus_name> <device_hex> <reg_hex> <length>", "Read data from the specified I2C device", cli_command_i2c_read},
};

static void cli_command_i2c_print_usage(void) {
    printf("Usage:\r\ni2c <cmd>\r\nCmd list:\r\n");
    for(size_t i = 0; i < COUNT_OF(i2c_cmds); i++) {
        const I2CCmd* c = &i2c_cmds[i];
        printf("\t%s %s - %s\r\n", c->name, c->arg_spec, c->description);
    }
}

void cli_command_i2c(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(context);
    FuriString* cmd = furi_string_alloc();
    bool handled = false;

    if(args_read_string_and_trim(args, cmd)) {
        const char* cmd_str = furi_string_get_cstr(cmd);
        for(size_t i = 0; i < COUNT_OF(i2c_cmds); i++) {
            const I2CCmd* c = &i2c_cmds[i];
            if(strcmp(cmd_str, c->name) == 0) {
                if(!c->execute(pipe, args)) {
                    printf("usage: i2c %s %s\r\n", c->name, c->arg_spec);
                }
                handled = true;
                break;
            }
        }
    }

    if(!handled) {
        cli_command_i2c_print_usage();
    }

    furi_string_free(cmd);
}
