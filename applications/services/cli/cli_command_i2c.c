#include <cli/args.h>
#include <containers/pipe.h>
#include <furi.h>
#include <furi_hal_i2c.h>
#include <furi_hal_i2c_config.h>

static const FuriHalI2cBusHandle* cli_command_i2c_resolve_bus(FuriString* bus_token) {
    const char* bus_name = furi_string_get_cstr(bus_token);

    if(strcmp(bus_name, "0") == 0 || strcmp(bus_name, "control") == 0) {
        return &furi_hal_i2c_handle_control;
    }

    if(strcmp(bus_name, "1") == 0 || strcmp(bus_name, "main") == 0) {
        return &furi_hal_i2c_handle_main;
    }

    return NULL;
}

static void cli_command_i2c_print_bus_list(void) {
    printf("0 - control\r\n");
    printf("1 - main\r\n");
}

static void cli_command_i2c_help(void) {
    printf("Usage:\r\n");
    printf("i2c list\r\n");
    printf("i2c search <0|control|1|main>\r\n");
    printf("i2c write <0|control|1|main> <device_hex> <prefix_hex> <data_hex>\r\n");
    printf("i2c read <0|control|1|main> <device_hex> <prefix_hex> <length>\r\n");
    printf("Examples:\r\n");
    printf("i2c list\r\n");
    printf("i2c read control 01 01 16\r\n");
    printf("i2c write main 20 00 1010\r\n");
}

static void cli_command_i2c_scan_bus(const FuriHalI2cBusHandle* handle, const char* bus_name) {
    furi_hal_i2c_acquire(handle);

    printf("Scanning %s bus (%s):\r\n", bus_name, furi_hal_i2c_bus_name(handle));
    printf("     0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\r\n");
    printf("    -----------------------------------------------\r\n");

    for(uint8_t addr = 0; addr < 128; addr++) {
        if(addr % 16 == 0) {
            printf("%02x | ", addr);
        }

        bool is_ready = furi_hal_i2c_device_ready(handle, addr, FURI_HAL_I2C_TIMEOUT_US);
        printf(is_ready ? "@" : ".");
        printf(addr % 16 == 15 ? "\r\n" : "  ");
    }

    furi_hal_i2c_release(handle);
}

static bool cli_command_i2c_parse_bus(FuriString* args, const FuriHalI2cBusHandle** bus_handle) {
    FuriString* bus_token = furi_string_alloc();
    bool is_parsed = false;

    do {
        if(!args_read_string_and_trim(args, bus_token)) {
            break;
        }

        *bus_handle = cli_command_i2c_resolve_bus(bus_token);
        if(*bus_handle == NULL) {
            break;
        }

        is_parsed = true;
    } while(false);

    furi_string_free(bus_token);
    return is_parsed;
}

static bool cli_command_i2c_parse_hex_u8(FuriString* args, uint8_t* value) {
    const size_t word_length = args_get_first_word_length(args);
    if(!args_read_hex_bytes(args, value, 1)) {
        return false;
    }

    furi_string_right(args, word_length);
    furi_string_trim(args);
    return true;
}

static bool cli_command_i2c_parse_write_payload(FuriString* args, uint8_t** data_buffer, size_t* data_size) {
    const size_t hex_size = args_get_first_word_length(args);
    if(hex_size == 0 || (hex_size % 2) != 0) {
        return false;
    }

    *data_size = hex_size / 2;
    *data_buffer = malloc(*data_size);

    if(!args_read_hex_bytes(args, *data_buffer, *data_size)) {
        free(*data_buffer);
        *data_buffer = NULL;
        *data_size = 0;
        return false;
    }

    furi_string_right(args, hex_size);
    furi_string_trim(args);
    return true;
}

void cli_command_i2c(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    UNUSED(context);

    if(args_length(args) == 0) {
        cli_command_i2c_help();
        return;
    }

    FuriString* subcommand = furi_string_alloc();
    if(!args_read_string_and_trim(args, subcommand)) {
        furi_string_free(subcommand);
        cli_command_i2c_help();
        return;
    }

    if(strcmp(furi_string_get_cstr(subcommand), "list") == 0) {
        cli_command_i2c_print_bus_list();
        furi_string_free(subcommand);
        return;
    }

    if(strcmp(furi_string_get_cstr(subcommand), "search") == 0) {
        const FuriHalI2cBusHandle* bus_handle = NULL;
        if(!cli_command_i2c_parse_bus(args, &bus_handle)) {
            furi_string_free(subcommand);
            cli_command_i2c_help();
            return;
        }

        const char* bus_name = (bus_handle == &furi_hal_i2c_handle_control) ? "control" : "main";
        cli_command_i2c_scan_bus(bus_handle, bus_name);
        furi_string_free(subcommand);
        return;
    }

    if(strcmp(furi_string_get_cstr(subcommand), "read") == 0) {
        const FuriHalI2cBusHandle* bus_handle = NULL;
        uint8_t device_address = 0;
        uint8_t register_prefix = 0;
        int read_length = 0;

        if(!cli_command_i2c_parse_bus(args, &bus_handle) || !cli_command_i2c_parse_hex_u8(args, &device_address) ||
           !cli_command_i2c_parse_hex_u8(args, &register_prefix) || !args_read_int_and_trim(args, &read_length) ||
           read_length <= 0 || read_length > 255) {
            furi_string_free(subcommand);
            cli_command_i2c_help();
            return;
        }

        uint8_t* read_buffer = malloc((size_t)read_length);

        furi_hal_i2c_acquire(bus_handle);
        int bytes_read = furi_hal_i2c_master_trx_blocking(
            bus_handle,
            device_address,
            &register_prefix,
            1,
            read_buffer,
            (size_t)read_length,
            FURI_HAL_I2C_TIMEOUT_US);
        furi_hal_i2c_release(bus_handle);

        if(bytes_read <= 0) {
            printf("Read failed\r\n");
            free(read_buffer);
            furi_string_free(subcommand);
            return;
        }

        for(int i = 0; i < bytes_read; i++) {
            printf("%02X", read_buffer[i]);
            if(i + 1 < bytes_read) {
                printf(" ");
            }
        }
        printf("\r\n");

        free(read_buffer);
        furi_string_free(subcommand);
        return;
    }

    if(strcmp(furi_string_get_cstr(subcommand), "write") == 0) {
        const FuriHalI2cBusHandle* bus_handle = NULL;
        uint8_t device_address = 0;
        uint8_t register_prefix = 0;
        uint8_t* write_payload = NULL;
        size_t write_payload_size = 0;

        if(!cli_command_i2c_parse_bus(args, &bus_handle) ||
           !cli_command_i2c_parse_hex_u8(args, &device_address) ||
           !cli_command_i2c_parse_hex_u8(args, &register_prefix) ||
           !cli_command_i2c_parse_write_payload(args, &write_payload, &write_payload_size)) {
            furi_string_free(subcommand);
            cli_command_i2c_help();
            return;
        }

        const size_t tx_size = write_payload_size + 1;
        uint8_t* tx_buffer = malloc(tx_size);

        tx_buffer[0] = register_prefix;
        memcpy(&tx_buffer[1], write_payload, write_payload_size);

        furi_hal_i2c_acquire(bus_handle);
        int bytes_written =
            furi_hal_i2c_master_tx_blocking(bus_handle, device_address, tx_buffer, tx_size, FURI_HAL_I2C_TIMEOUT_US);
        furi_hal_i2c_release(bus_handle);

        if(bytes_written != (int)tx_size) {
            printf("Write failed\r\n");
        }

        free(tx_buffer);
        free(write_payload);
        furi_string_free(subcommand);
        return;
    }

    furi_string_free(subcommand);
    cli_command_i2c_help();
}
