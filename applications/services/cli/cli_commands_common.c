#include "cli_commands_common.h"

#include <furi_hal.h>
#include <cli/cli_ansi.h>
#include <cli/args.h>
#include <cli/cli_command.h>
#include <time.h>
#include <furi_bsp.h>
#include <led/led.h>
#include <furi_hal_clock.h>
#include <furi_hal_i2c_types.h>
#include <furi_hal_i2c_config.h>
#include <furi_hal_otp.h>
#include <toolbox/hex.h>
#include <toolbox/strint.h>
#include <stdlib.h>
#include <string.h>

#define CLI_I2C_MAX_TRANSFER_SIZE 1024U

typedef struct {
    const char* name;
    const FuriHalI2cBusHandle* handle;
} CliI2cBusDescription;

static const CliI2cBusDescription cli_i2c_buses[] = {
    {
        .name = "control",
        .handle = &furi_hal_i2c_handle_control,
    },
    {
        .name = "main",
        .handle = &furi_hal_i2c_handle_main,
    },
};

void cli_command_uptime(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    UNUSED(args);
    UNUSED(context);
    uint32_t uptime = furi_get_tick() / furi_kernel_get_tick_frequency();
    printf("Uptime: %luh%lum%lus", uptime / 60 / 60, uptime / 60 % 60, uptime % 60);
}

static void cli_command_log_tx_callback(const uint8_t* buffer, size_t size, void* context) {
    PipeSide* pipe = context;
    pipe_send(pipe, buffer, size);
}

static bool cli_command_log_level_set_from_string(FuriString* level) {
    FuriLogLevel log_level;
    if(furi_log_level_from_string(furi_string_get_cstr(level), &log_level)) {
        furi_log_set_level(log_level);
        return true;
    } else {
        printf("<log> — start logging using the current level from the system settings\r\n");
        printf("<log error> — only critical errors and other important messages\r\n");
        printf("<log warn> — non-critical errors and warnings including <log error>\r\n");
        printf("<log info> — non-critical information including <log warn>\r\n");
        printf("<log default> — the default system log level (equivalent to <log info>)\r\n");
        printf(
            "<log debug> — debug information including <log info> (may impact system performance)\r\n");
        printf(
            "<log trace> — system traces including <log debug> (may impact system performance)\r\n");
    }
    return false;
}

void cli_command_log(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(context);
    FuriLogLevel previous_level = furi_log_get_level();
    bool restore_log_level = false;

    if(furi_string_size(args) > 0) {
        if(!cli_command_log_level_set_from_string(args)) {
            return;
        }
        restore_log_level = true;
    }

    const char* current_level;
    furi_log_level_to_string(furi_log_get_level(), &current_level);
    printf("Current log level: %s\r\n", current_level);

    FuriLogHandler log_handler = {
        .callback = cli_command_log_tx_callback,
        .context = pipe,
    };

    furi_log_add_handler(log_handler);

    printf("Use <log ?> to list available log levels\r\n");
    printf("Press CTRL+C to stop...\r\n");
    while(!cli_is_pipe_broken_or_is_etx_next_char(pipe)) {
        furi_delay_ms(100);
    }

    furi_log_remove_handler(log_handler);

    if(restore_log_level) {
        // There will be strange behaviour if log level is set from settings while log command is running
        furi_log_set_level(previous_level);
    }
}

void cli_command_top(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(context);

    int interval = 1000;
    args_read_int_and_trim(args, &interval);
    
    printf(ANSI_ERASE_DISPLAY(ANSI_ERASE_ENTIRE)); // Clear display
    
    FuriThreadList* thread_list = furi_thread_list_alloc();
    while(!cli_is_pipe_broken_or_is_etx_next_char(pipe)) {
        uint32_t tick = furi_get_tick();
        furi_thread_enumerate(thread_list);

        if(interval) printf(ANSI_CURSOR_POS("1", "1"));

        uint32_t uptime = tick / furi_kernel_get_tick_frequency();
        printf(
            "Threads: %zu, ISR Time: %0.2f%%, Uptime: %luh%lum%lus" ANSI_ERASE_LINE(
                ANSI_ERASE_FROM_CURSOR_TO_END) "\r\n",
            furi_thread_list_size(thread_list),
            (double)furi_thread_list_get_isr_time(thread_list),
            uptime / 60 / 60,
            uptime / 60 % 60,
            uptime % 60);

        printf(
            "Heap: total %zu, free %zu, minimum %zu, max block %zu" ANSI_ERASE_LINE(
                ANSI_ERASE_FROM_CURSOR_TO_END) "\r\n" ANSI_ERASE_LINE(ANSI_ERASE_FROM_CURSOR_TO_END) "\r\n",
            memmgr_get_total_heap(),
            memmgr_get_free_heap(),
            memmgr_get_minimum_free_heap(),
            memmgr_heap_get_max_free_block());

        printf(
            "%-25s %-20s %-10s %5s %12s %6s %10s %7s %5s" ANSI_ERASE_LINE(
                ANSI_ERASE_FROM_CURSOR_TO_END) "\r\n",
            "AppID",
            "Name",
            "State",
            "Prio",
            "Stack start",
            "Stack",
            "Stack Min",
            "Heap",
            "%CPU");

        for(size_t i = 0; i < furi_thread_list_size(thread_list); i++) {
            const FuriThreadListItem* item = furi_thread_list_get_at(thread_list, i);
            printf(
                "%-25s %-20s %-10s %5d   0x%08lx %6lu %10lu %7zu %5.1f" ANSI_ERASE_LINE(
                    ANSI_ERASE_FROM_CURSOR_TO_END) "\r\n",
                item->app_id,
                item->name,
                item->state,
                item->priority,
                item->stack_address,
                item->stack_size,
                item->stack_min_free,
                item->heap,
                (double)item->cpu);
        }

        printf(ANSI_ERASE_DISPLAY(ANSI_ERASE_FROM_CURSOR_TO_END));
        fflush(stdout);

        if(interval > 0) {
            furi_delay_ms(interval);
        } else {
            break;
        }
    }
    furi_thread_list_free(thread_list);
}


void cli_command_free(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    UNUSED(args);
    UNUSED(context);

    printf("Free heap size: %zu\r\n", memmgr_get_free_heap());
    printf("Total heap size: %zu\r\n", memmgr_get_total_heap());
    printf("Minimum heap size: %zu\r\n", memmgr_get_minimum_free_heap());
    printf("Maximum heap block: %zu\r\n", memmgr_heap_get_max_free_block());

    printf("Pool free: %zu\r\n", memmgr_pool_get_free());
    printf("Maximum pool block: %zu\r\n", memmgr_pool_get_max_block());
}

void cli_command_free_blocks(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    UNUSED(args);
    UNUSED(context);

    memmgr_heap_printf_free_blocks();
}

static void cli_scan_i2c_bus(const FuriHalI2cBusHandle* handle, const char* bus_name) {
    furi_hal_i2c_acquire(handle);
    furi_check(handle);

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

static void cli_command_i2c_help(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    UNUSED(args);
    UNUSED(context);

    printf(
        "Usage:\r\n"
        "i2c\r\n"
        "i2c list\r\n"
        "i2c search <0|control|1|main>\r\n"
        "i2c write <0|control|1|main> <device_hex> <prefix_hex> <data_hex>\r\n"
        "i2c read <0|control|1|main> <device_hex> <prefix_hex> <length>\r\n"
        "Examples:\r\n"
        "i2c list\r\n"
        "i2c search control\r\n"
        "i2c write control 01 01 02030404\r\n"
        "i2c read control 01 01 16\r\n");
}

static void cli_command_i2c_list_buses(void) {
    for(size_t i = 0; i < COUNT_OF(cli_i2c_buses); i++) {
        printf("%u - %s\r\n", i, cli_i2c_buses[i].name);
    }
}

static void cli_command_i2c_scan_all_buses(void) {
    cli_scan_i2c_bus(cli_i2c_buses[0].handle, cli_i2c_buses[0].name);
    printf("\r\n");
    cli_scan_i2c_bus(cli_i2c_buses[1].handle, cli_i2c_buses[1].name);
}

static bool cli_command_i2c_parse_uint32(const char* word, uint8_t base, uint32_t* value) {
    char* end = NULL;
    if(strint_to_uint32(word, &end, value, base) != StrintParseNoError) {
        return false;
    }

    return end != NULL && *end == '\0';
}

static bool cli_command_i2c_parse_hex_uint32(const char* word, uint32_t* value) {
    const bool has_prefix =
        (word[0] == '0') && ((word[1] == 'x') || (word[1] == 'X') || (word[1] == 'b') || (word[1] == 'B'));

    return cli_command_i2c_parse_uint32(word, has_prefix ? 0 : 16, value);
}

static bool cli_command_i2c_resolve_bus(
    FuriString* args,
    const CliI2cBusDescription** bus_description) {
    bool success = false;
    FuriString* word = furi_string_alloc();

    do {
        if(!args_read_string_and_trim(args, word)) {
            break;
        }

        uint32_t bus_index = 0;
        if(cli_command_i2c_parse_uint32(furi_string_get_cstr(word), 10, &bus_index) &&
           bus_index < COUNT_OF(cli_i2c_buses)) {
            *bus_description = &cli_i2c_buses[bus_index];
            success = true;
            break;
        }

        for(size_t i = 0; i < COUNT_OF(cli_i2c_buses); i++) {
            if(furi_string_equal_str(word, cli_i2c_buses[i].name)) {
                *bus_description = &cli_i2c_buses[i];
                success = true;
                break;
            }
        }
    } while(false);

    furi_string_free(word);
    return success;
}

static bool cli_command_i2c_parse_device_address(FuriString* args, uint8_t* device_address) {
    bool success = false;
    FuriString* word = furi_string_alloc();

    do {
        if(!args_read_string_and_trim(args, word)) {
            break;
        }

        uint32_t value = 0;
        if(!cli_command_i2c_parse_hex_uint32(furi_string_get_cstr(word), &value) ||
           value > UINT8_MAX) {
            break;
        }

        *device_address = value;
        success = true;
    } while(false);

    furi_string_free(word);
    return success;
}

static bool cli_command_i2c_parse_hex_buffer(
    FuriString* args,
    uint8_t** buffer,
    size_t* buffer_size) {
    bool success = false;
    FuriString* word = furi_string_alloc();
    uint8_t* data = NULL;

    *buffer = NULL;
    *buffer_size = 0;

    do {
        if(!args_read_string_and_trim(args, word)) {
            break;
        }

        size_t word_size = furi_string_size(word);
        if((word_size == 0) || ((word_size % 2) != 0)) {
            break;
        }

        size_t byte_count = word_size / 2;
        if(byte_count > CLI_I2C_MAX_TRANSFER_SIZE) {
            break;
        }

        data = malloc(byte_count);
        if(data == NULL) {
            break;
        }

        size_t bytes_written = 0;
        if(!hex_string_to_bytes(word, data, byte_count, &bytes_written) ||
           bytes_written != byte_count) {
            break;
        }

        *buffer = data;
        *buffer_size = byte_count;
        data = NULL;
        success = true;
    } while(false);

    free(data);
    furi_string_free(word);
    return success;
}

static bool cli_command_i2c_parse_read_length(FuriString* args, size_t* read_length) {
    bool success = false;
    FuriString* word = furi_string_alloc();

    do {
        if(!args_read_string_and_trim(args, word)) {
            break;
        }

        uint32_t value = 0;
        if(!cli_command_i2c_parse_uint32(furi_string_get_cstr(word), 0, &value) ||
           (value == 0) || (value > CLI_I2C_MAX_TRANSFER_SIZE)) {
            break;
        }

        *read_length = value;
        success = true;
    } while(false);

    furi_string_free(word);
    return success;
}

static void cli_command_i2c_print_hex_buffer(const uint8_t* buffer, size_t buffer_size) {
    for(size_t i = 0; i < buffer_size; i++) {
        printf("%02x", buffer[i]);
    }
    printf("\r\n");
}

static void cli_command_i2c_write(FuriString* args) {
    const CliI2cBusDescription* bus_description = NULL;
    uint8_t device_address = 0;
    uint8_t* prefix = NULL;
    size_t prefix_size = 0;
    uint8_t* data = NULL;
    size_t data_size = 0;
    uint8_t* tx_buffer = NULL;
    size_t tx_size = 0;

    do {
        if(!cli_command_i2c_resolve_bus(args, &bus_description) ||
           !cli_command_i2c_parse_device_address(args, &device_address) ||
           !cli_command_i2c_parse_hex_buffer(args, &prefix, &prefix_size) ||
           !cli_command_i2c_parse_hex_buffer(args, &data, &data_size) ||
           (args_length(args) != 0)) {
            printf("Invalid arguments. Use 'i2c' to see help.\r\n");
            break;
        }

        tx_size = prefix_size + data_size;
        tx_buffer = malloc(tx_size);
        if(tx_buffer == NULL) {
            printf("Not enough memory.\r\n");
            break;
        }

        memcpy(tx_buffer, prefix, prefix_size);
        memcpy(tx_buffer + prefix_size, data, data_size);

        furi_hal_i2c_acquire(bus_description->handle);
        int status = furi_hal_i2c_master_tx_blocking(
            bus_description->handle,
            device_address,
            tx_buffer,
            tx_size,
            FURI_HAL_I2C_TIMEOUT_US);
        furi_hal_i2c_release(bus_description->handle);

        if(status < 0 || (size_t)status != tx_size) {
            printf(
                "I2C write failed on %s bus for device 0x%02x (status %d).\r\n",
                bus_description->name,
                device_address,
                status);
            break;
        }

        printf(
            "Wrote %u byte(s) to 0x%02x on %s bus.\r\n",
            (unsigned)data_size,
            device_address,
            bus_description->name);
    } while(false);

    free(tx_buffer);
    free(data);
    free(prefix);
}

static void cli_command_i2c_read(FuriString* args) {
    const CliI2cBusDescription* bus_description = NULL;
    uint8_t device_address = 0;
    uint8_t* prefix = NULL;
    size_t prefix_size = 0;
    size_t read_length = 0;
    uint8_t* rx_buffer = NULL;

    do {
        if(!cli_command_i2c_resolve_bus(args, &bus_description) ||
           !cli_command_i2c_parse_device_address(args, &device_address) ||
           !cli_command_i2c_parse_hex_buffer(args, &prefix, &prefix_size) ||
           !cli_command_i2c_parse_read_length(args, &read_length) ||
           (args_length(args) != 0)) {
            printf("Invalid arguments. Use 'i2c' to see help.\r\n");
            break;
        }

        rx_buffer = malloc(read_length);
        if(rx_buffer == NULL) {
            printf("Not enough memory.\r\n");
            break;
        }

        furi_hal_i2c_acquire(bus_description->handle);
        int status = furi_hal_i2c_master_trx_blocking(
            bus_description->handle,
            device_address,
            prefix,
            prefix_size,
            rx_buffer,
            read_length,
            FURI_HAL_I2C_TIMEOUT_US);
        furi_hal_i2c_release(bus_description->handle);

        if(status < 0 || (size_t)status != read_length) {
            printf(
                "I2C read failed on %s bus for device 0x%02x (status %d).\r\n",
                bus_description->name,
                device_address,
                status);
            break;
        }

        cli_command_i2c_print_hex_buffer(rx_buffer, read_length);
    } while(false);

    free(rx_buffer);
    free(prefix);
}

void cli_command_i2c(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    UNUSED(context);

    if(args_length(args) == 0) {
        cli_command_i2c_scan_all_buses();
        return;
    }

    FuriString* command = furi_string_alloc();
    if(!args_read_string_and_trim(args, command)) {
        furi_string_free(command);
        cli_command_i2c_help(pipe, args, context);
        return;
    }

    if(furi_string_equal_str(command, "list")) {
        if(args_length(args) == 0) {
            cli_command_i2c_list_buses();
        } else {
            cli_command_i2c_help(pipe, args, context);
        }
    } else if(furi_string_equal_str(command, "search")) {
        const CliI2cBusDescription* bus_description = NULL;
        if(cli_command_i2c_resolve_bus(args, &bus_description) && (args_length(args) == 0)) {
            cli_scan_i2c_bus(bus_description->handle, bus_description->name);
        } else {
            cli_command_i2c_help(pipe, args, context);
        }
    } else if(furi_string_equal_str(command, "write")) {
        cli_command_i2c_write(args);
    } else if(furi_string_equal_str(command, "read")) {
        cli_command_i2c_read(args);
    } else {
        cli_command_i2c_help(pipe, args, context);
    }

    furi_string_free(command);
}

static void cli_command_expander_ext_help(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    UNUSED(args);
    UNUSED(context);
    printf(
        "Usage: expander_ext <GPIO_OUT_NUMBER> <VALUE>\r\n"
        "Where <GPIO_OUT_NUMBER> is:\r\n"
        "\tUSB2.0_SEL \t\t0 \r\n"
        "\tHUB_PWR_EN \t\t1\r\n"
        "\tTYPE-A_UP_SW_EN \t2 \r\n"
        "\tVCC5V0_DEVICE_S0_EN \t3 \r\n"
        "\tVCC5V0_SYS_S5_EN \t4 \r\n"
        "\tGPIO_5V0_EN \t\t5 \r\n"
        "\tGPIO_3V3_EN \t\t6 \r\n"
        "\tEXPANDER_P17 \t\t7 \r\n"
        "Where <VALUE> is:\r\n"
        "\tSet output low \t\t0 \r\n"
        "\tSet output high \t\t1\r\n");
}

static OutputExpMain cli_command_expander_ext_set(OutputExpMain expander_gpio_out_read, OutputExpMain expander_gpio_out, int expander_gpio_out_value) {
    if(expander_gpio_out_value == 1) {
        expander_gpio_out_read |= expander_gpio_out;
    } else {
        expander_gpio_out_read &= ~expander_gpio_out;
    }
    return expander_gpio_out_read;
}

void cli_command_expander_ext(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    UNUSED(context);

    if(furi_string_size(args) < 2) {
        cli_command_expander_ext_help(pipe, args, context);
        return;
    }

    int expander_gpio_out_number = 0;
    if(!args_read_int_and_trim(args, &expander_gpio_out_number)) {
        cli_command_expander_ext_help(pipe, args, context);
        return;
    }
    if(expander_gpio_out_number < 0 || expander_gpio_out_number > 7) {
        cli_command_expander_ext_help(pipe, args, context);
        return;
    }

    int expander_gpio_out_value = 0;
    if(!args_read_int_and_trim(args, &expander_gpio_out_value)) {
        cli_command_expander_ext_help(pipe, args, context);
        return;
    }
    if(expander_gpio_out_value != 0 && expander_gpio_out_value != 1) {
        cli_command_expander_ext_help(pipe, args, context);
        return;
    }

    printf("Setting expander GPIO out %d to %d\r\n", expander_gpio_out_number, expander_gpio_out_value);

    OutputExpMain output = furi_bsp_expander_main_read_output();

    switch(expander_gpio_out_number) {
    case 0:
        output = cli_command_expander_ext_set(output, OutputExpMainUsb20Sel, expander_gpio_out_value);
        break;
    case 1:
        output = cli_command_expander_ext_set(output, OutputExpMainHubPwrEn, expander_gpio_out_value);
        break;
    case 2:
        output = cli_command_expander_ext_set(output, OutputExpMainTypeAUpSwEn, expander_gpio_out_value);
        break;
    case 3:
        output = cli_command_expander_ext_set(output, OutputExpMainVcc5v0DevS0En, expander_gpio_out_value);
        break;
    case 4:
        output = cli_command_expander_ext_set(output, OutputExpMainVcc5v0SysS5En, expander_gpio_out_value);
        break;
    case 5:
        output = cli_command_expander_ext_set(output, OutputExpMainGpio5v0En, expander_gpio_out_value);
        break;
    case 6:
        output = cli_command_expander_ext_set(output, OutputExpMainGpio3v3En, expander_gpio_out_value);
        break;
    case 7:
        output = cli_command_expander_ext_set(output, OutputExpMainExpander17, expander_gpio_out_value);
        break;
    }

    furi_bsp_expander_main_write_output(output);
}

FuriHalClockSource cli_clock_sources[] = {
    FuriHalClockSourceNone,
    FuriHalClockSourcePllSys,
    FuriHalClockSourcePllUsb,
    FuriHalClockSourcePllUsbPrimaryRefOpcg,
    FuriHalClockSourceRosc,
    FuriHalClockSourceXosc,
    FuriHalClockSourceLposc,
    FuriHalClockSourceSys,
    FuriHalClockSourceUsb,
    FuriHalClockSourceAdc,
    FuriHalClockSourceRef,
    FuriHalClockSourcePeri,
    FuriHalClockSourceHstx,
    FuriHalClockSourceOtp2fc,
    FuriHalClockSourceMax,
};

static void cli_command_clock_out_help(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    UNUSED(args);
    UNUSED(context);
    printf(
        "Usage: Outputting clock to GPIO13 with divider <CLOCK_SOURCE> <DIV>\r\n"
        "Where <CLOCK_SOURCE> is:\r\n"
        "\tNone \t\t\t0\r\n"
        "\tPllSys \t\t\t1\r\n"
        "\tPllUsb \t\t\t2\r\n"
        "\tPllUsbPrimaryRefOpcg \t3\r\n"
        "\tRosc \t\t\t4\r\n"
        "\tXosc \t\t\t5\r\n"
        "\tLposc \t\t\t6\r\n"
        "\tSys \t\t\t7\r\n"
        "\tUsb \t\t\t8\r\n"
        "\tAdc \t\t\t9\r\n"
        "\tRef \t\t\t10\r\n"
        "\tPeri \t\t\t11\r\n"
        "\tHstx \t\t\t12\r\n"
        "\tOtp2fc \t\t\t13\r\n");
    printf(
        "Where <DIV> is: a divider for the clock, for example 1, 2, 4, 8, \r\n"
        "\tetc. The output frequency will be CLOCK_SOURCE_FREQ / DIV\r\n");
}

void cli_command_clock_out(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    UNUSED(context);

    if(furi_string_size(args) < 1) {
        cli_command_clock_out_help(pipe, args, context);
        return;
    }

    int source = 0;
    int div = 0;
    if(!args_read_int_and_trim(args, &source)) {
        cli_command_clock_out_help(pipe, args, context);
        return;
    }
    if(source < 0 || source >= sizeof(cli_clock_sources) / sizeof(FuriHalClockSource)) {
        cli_command_clock_out_help(pipe, args, context);
        return;
    }

    if(furi_string_size(args) < 1 && cli_clock_sources[source] != FuriHalClockSourceNone) {
        cli_command_clock_out_help(pipe, args, context);
        return;
    }

    if(cli_clock_sources[source] != FuriHalClockSourceNone) {
        if(!args_read_int_and_trim(args, &div)) {
            cli_command_clock_out_help(pipe, args, context);
            return;
        }
        if(div < 0) {
            cli_command_clock_out_help(pipe, args, context);
            return;
        }
        printf("Outputting clock %d to GPIO13 with divider %d\r\n", source, div);
    } else {
        printf("Disabling clock output to GPIO13\r\n");
    }

    furi_hal_clock_out_to_gpio13(cli_clock_sources[source], (float)div);
}

static void cli_command_otp_help(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    UNUSED(args);
    UNUSED(context);
    printf(
        "otp <OTP_ACTION>\r\n"
        "Where <OTP_ACTION> is:\r\n"
        "\twhitelabel <FIRMWARE_ID> <BODY_ID> <CONNECTIVITY_ID>\r\n"
        "");
}

void cli_command_otp(PipeSide* pipe, FuriString* args, void* context) {
    FuriString* action = furi_string_alloc();
    do {
        if(!args_read_string_and_trim(args, action)) {
            cli_command_otp_help(pipe, args, context);
            break;
        }

        if(furi_string_cmp(action, "whitelabel") != 0) {
            cli_command_otp_help(pipe, args, context);
            break;
        }

        int firmware_id, body_id, connectivity_id;
        if(!args_read_int_and_trim(args, &firmware_id) || !args_read_int_and_trim(args, &body_id) || !args_read_int_and_trim(args, &connectivity_id)) {
            cli_command_otp_help(pipe, args, context);
            break;
        }

        if(firmware_id < 0 || body_id < 0 || connectivity_id < 0) {
            cli_command_otp_help(pipe, args, context);
            break;
        }

        printf("Programming USB white label in OTP with F%dB%dC%d\r\n", firmware_id, body_id, connectivity_id);

        if(furi_hal_otp_usb_white_label_valid()) {
            printf("USB white label is already programmed in OTP, it cannot be programmed again\r\n");
            break;
        }

        FuriHalOtpUsbWhiteLabelError error = furi_hal_otp_write_usb_white_label(firmware_id, body_id, connectivity_id);
        if(error == FuriHalOtpUsbWhiteLabelErrorNone) {
            printf("USB white label written to OTP successfully\r\n");
        } else {
            printf("Failed to write USB white label to OTP: %d\r\n", error);
        }
    } while(0);
    furi_string_free(action);
}