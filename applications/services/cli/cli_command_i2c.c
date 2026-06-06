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
} I2cCmd;

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
    printf(" 0 - %s\r\n", I2C_BUS_CONTROL);
    printf(" 1 - %s\r\n", I2C_BUS_MAIN);
    return true;
}

static bool cli_command_i2c_search(PipeSide* pipe, FuriString* args) {
    UNUSED(pipe);

    if(strcmp(furi_string_get_cstr(args), I2C_BUS_CONTROL) == 0 || strcmp(furi_string_get_cstr(args), "0") == 0) {
        cli_command_i2c_scan_bus(&furi_hal_i2c_handle_control, I2C_BUS_CONTROL);
    } else if(strcmp(furi_string_get_cstr(args), I2C_BUS_MAIN) == 0 || strcmp(furi_string_get_cstr(args), "1") == 0) {
        cli_command_i2c_scan_bus(&furi_hal_i2c_handle_main, I2C_BUS_MAIN);
    } else {
        printf(ANSI_FG_RED "Unknown I2C bus:" ANSI_RESET " %s\r\n", furi_string_get_cstr(args));
        return false;
    }
    return true;
}

static bool cli_command_i2c_write(PipeSide* pipe, FuriString* args) {
    UNUSED(pipe);
    const FuriHalI2cBusHandle* handle;
    uint8_t device_address;
    uint8_t device_register;
    uint8_t* data;
    size_t data_length;
    FuriString* bus_name = furi_string_alloc();

    if(furi_string_size(args)) {
        args_read_string_and_trim(args, bus_name);
        if(strcmp(furi_string_get_cstr(bus_name), I2C_BUS_CONTROL) == 0 || strcmp(furi_string_get_cstr(bus_name), "0") == 0) {
            handle = &furi_hal_i2c_handle_control;
        } else if(strcmp(furi_string_get_cstr(bus_name), I2C_BUS_MAIN) == 0 || strcmp(furi_string_get_cstr(bus_name), "1") == 0) {
            handle = &furi_hal_i2c_handle_main;
        } else {
            printf(ANSI_FG_RED "Unknown I2C bus:" ANSI_RESET " %s\r\n", furi_string_get_cstr(bus_name));
            furi_string_free(bus_name);
            return false;
        }

        char* args_cstr = (char*)furi_string_get_cstr(args);
        StrintParseError parse_err = StrintParseNoError;
        parse_err |= strint_to_uint8(args_cstr, &args_cstr, &device_address, 16);
        parse_err |= strint_to_uint8(args_cstr, &args_cstr, &device_register, 16);
        if(parse_err || (device_address > 0x7F) || ((device_address & 0x78) == 0) || ((device_address & 0x78) == 0x78)) {
            furi_string_free(bus_name);
            return false;
        }

        furi_string_printf(args, "%s", args_cstr);
        furi_string_trim(args);

        data_length = furi_string_size(args) / 2;
        data = malloc(data_length + 1); // +1 for device register
        data[0] = device_register;
        if(!args_read_hex_bytes(args, data + 1, data_length)) {
            printf(ANSI_FG_RED "Failed" ANSI_RESET " to read hex data\r\n");
            free(data);
            furi_string_free(bus_name);
            return false;
        }

    } else {
        furi_string_free(bus_name);
        return false;
    }

    furi_hal_i2c_acquire(handle);
    int success = furi_hal_i2c_master_tx_blocking(handle, device_address, data, data_length + 1, FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(handle);

    if(success >= PICO_OK) {
        printf(ANSI_FG_GREEN "Written" ANSI_RESET " to device 0x%02X register 0x%02X:", device_address, device_register);
        for(size_t i = 1; i < data_length + 1; i++) {
            printf(" %02X", data[i]);
        }
        printf("\r\n");
    } else {
        printf(
            ANSI_FG_RED "Failed" ANSI_RESET " to write to bus %s device 0x%02X register 0x%02X\r\n",
            furi_string_get_cstr(bus_name),
            device_address,
            device_register);
    }

    free(data);
    furi_string_free(bus_name);
    return true;
}

static bool cli_command_i2c_read(PipeSide* pipe, FuriString* args) {
    UNUSED(pipe);

    const FuriHalI2cBusHandle* handle;
    uint8_t device_address;
    uint8_t device_register;
    uint8_t length;
    FuriString* bus_name = furi_string_alloc();

    if(furi_string_size(args)) {
        args_read_string_and_trim(args, bus_name);
        if(strcmp(furi_string_get_cstr(bus_name), I2C_BUS_CONTROL) == 0 || strcmp(furi_string_get_cstr(bus_name), "0") == 0) {
            handle = &furi_hal_i2c_handle_control;
        } else if(strcmp(furi_string_get_cstr(bus_name), I2C_BUS_MAIN) == 0 || strcmp(furi_string_get_cstr(bus_name), "1") == 0) {
            handle = &furi_hal_i2c_handle_main;
        } else {
            printf(ANSI_FG_RED "Unknown I2C bus:" ANSI_RESET " %s\r\n", furi_string_get_cstr(bus_name));
            furi_string_free(bus_name);
            return false;
        }

        char* args_cstr = (char*)furi_string_get_cstr(args);
        StrintParseError parse_err = StrintParseNoError;
        parse_err |= strint_to_uint8(args_cstr, &args_cstr, &device_address, 16);
        parse_err |= strint_to_uint8(args_cstr, &args_cstr, &device_register, 16);
        parse_err |= strint_to_uint8(args_cstr, &args_cstr, &length, 10);
        if(parse_err || (device_address > 0x7F) || ((device_address & 0x78) == 0) || ((device_address & 0x78) == 0x78)) {
            furi_string_free(bus_name);
            return false;
        }
    } else {
        furi_string_free(bus_name);
        return false;
    }

    uint8_t* buffer = malloc(length);

    furi_hal_i2c_acquire(handle);
    int success = furi_hal_i2c_master_trx_blocking(handle, device_address, &device_register, 1, buffer, length, FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(handle);

    if(success >= PICO_OK) {
        printf(ANSI_FG_GREEN "Read" ANSI_RESET " from device 0x%02X register 0x%02X:", device_address, device_register);
        for(size_t i = 0; i < length; i++) {
            printf(" %02X", buffer[i]);
        }
        printf("\r\n");
    } else {
        printf(
            ANSI_FG_RED "Failed" ANSI_RESET " to read bus %s from device 0x%02X register 0x%02X\r\n",
            furi_string_get_cstr(bus_name),
            device_address,
            device_register);
    }

    free(buffer);
    furi_string_free(bus_name);
    return true;
}

static const I2cCmd i2c_cmds[] = {
    {"list", "", "List available I2C buses", cli_command_i2c_list},
    {"search", "<bus_name>", "Search for devices on the specified I2C bus", cli_command_i2c_search},
    {"write", "<bus_name> <device_hex> <reg_hex> <data_hex>", "Write data to the specified I2C device", cli_command_i2c_write},
    {"read", "<bus_name> <device_hex> <reg_hex> <length>", "Read data from the specified I2C device", cli_command_i2c_read},
};

static void cli_command_i2c_print_usage(void) {
    printf("Usage:\r\ni2c <cmd>\r\nCmd list:\r\n");
    for(size_t i = 0; i < COUNT_OF(i2c_cmds); i++) {
        const I2cCmd* c = &i2c_cmds[i];
        printf("\t%s %s - %s\r\n", c->name, c->arg_spec, c->description);
    }
}

void cli_command_i2c(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(context);
    FuriString* cmd = furi_string_alloc();
    bool handled = false;

    if(args_read_string_and_trim(args, cmd)) {
        const char* cmd_str = furi_string_get_cstr(cmd);
        const char* arg_str = furi_string_get_cstr(args);
        for(size_t i = 0; i < COUNT_OF(i2c_cmds); i++) {
            const I2cCmd* c = &i2c_cmds[i];
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
