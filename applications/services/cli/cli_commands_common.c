#include "cli_commands_common.h"

#include <furi_hal.h>
#include <pico/stdio.h>
#include <cli/cli_ansi.h>
#include <cli/args.h>
#include <cli/cli_command.h>
#include <time.h>
#include <furi_bsp.h>
#include <led/led.h>
#include <furi_hal_clock.h>
#include <furi_hal_otp.h>
#include <toolbox/strint.h>
#include <ctype.h>
#include <furi_hal_nvm.h>

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
        printf("<log debug> — debug information including <log info> (may impact system performance)\r\n");
        printf("<log trace> — system traces including <log debug> (may impact system performance)\r\n");
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
            "Threads: %zu, ISR Time: %0.2f%%, Uptime: %luh%lum%lus" ANSI_ERASE_LINE(ANSI_ERASE_FROM_CURSOR_TO_END) "\r\n",
            furi_thread_list_size(thread_list),
            (double)furi_thread_list_get_isr_time(thread_list),
            uptime / 60 / 60,
            uptime / 60 % 60,
            uptime % 60);

        printf(
            "Heap: total %zu, free %zu, minimum %zu, max block %zu" ANSI_ERASE_LINE(ANSI_ERASE_FROM_CURSOR_TO_END) "\r\n" ANSI_ERASE_LINE(
                ANSI_ERASE_FROM_CURSOR_TO_END) "\r\n",
            memmgr_get_total_heap(),
            memmgr_get_free_heap(),
            memmgr_get_minimum_free_heap(),
            memmgr_heap_get_max_free_block());

        printf(
            "%-25s %-20s %-10s %5s %12s %6s %10s %7s %5s" ANSI_ERASE_LINE(ANSI_ERASE_FROM_CURSOR_TO_END) "\r\n",
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
                "%-25s %-20s %-10s %5d   0x%08lx %6lu %10lu %7zu %5.1f" ANSI_ERASE_LINE(ANSI_ERASE_FROM_CURSOR_TO_END) "\r\n",
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
        stdio_flush();

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

static void cli_command_expander_ext_help(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    UNUSED(args);
    UNUSED(context);
    printf(
        "Usage:  <GPIO_OUT_NUMBER> <VALUE>\r\n"
        "Where <GPIO_OUT_NUMBER> is:\r\n"
        "\tUSB2.0_SEL \t\t0 \r\n"
        "\tHUB_PWR_EN \t\t1\r\n"
        "\tTYPE-A_UP_SW_EN \t2 \r\n"
        "\tVCC5V0_DEVICE_S0_EN \t3 \r\n"
        "\tVCC5V0_SYS_S5_EN \t4 \r\n"
        "\tGPIO_5V0_EN \t\t5 \r\n"
        "\tGPIO_3V3_EN \t\t6 \r\n"
        "\tEXPANDER_P17 \t\t7 \r\n"
        "\tnMUX_EN \t\t8 \r\n"
        "Where <VALUE> is:\r\n"
        "\tSet output low \t\t0 \r\n"
        "\tSet output high \t1\r\n");
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
    if(expander_gpio_out_number < 0 || expander_gpio_out_number > 8) {
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
        output = cli_command_expander_ext_set(output, OutputExpMainMaskromEn, expander_gpio_out_value);
        break;
    case 8:
        output = cli_command_expander_ext_set(output, OutputExpMainNMuxEn, expander_gpio_out_value);
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

static void cli_command_otp_help(void) {
    printf(
        "otp info\r\n"
        "otp device <NAME> <VERSION> <TARGET> <BODY> <CONNECT> <DATE>\r\n"
        "\tNAME: up to %u chars of [A-Za-z0-9._-]; TARGET: the board's FW_TARGET number (f2 is 2);\r\n"
        "\tDATE: seconds since the epoch.\r\n"
        "\tAlso writes the USB white label (board id F<TARGET>B<BODY>C<CONNECT>) if it is not there.\r\n"
        "otp mac <ETH0> <ETH1> <USB> <WIFI_2G4_STA> <WIFI_5G_STA> <WIFI_2G4_AP> <WIFI_5G_AP> <BT>\r\n"
        "\teach as aa:bb:cc:dd:ee:ff\r\n"
        "device and mac write OTP once and lock it: this cannot be undone.\r\n",
        FURI_HAL_OTP_DEVICE_NAME_SIZE - 1);
}

static void cli_command_otp_info(void) {
    printf("USB white label: %s\r\n", furi_hal_otp_usb_white_label_valid() ? "written" : "not written");

    FuriHalOtpDevice device;
    if(furi_hal_otp_device_read(&device)) {
        printf(
            "Device: %s, version %u, F%uB%uC%u, date %llu\r\n",
            device.name,
            device.version,
            device.target,
            device.body,
            device.connect,
            (unsigned long long)device.timestamp);
    } else {
        printf("Device: not written\r\n");
    }

    FuriHalOtpMacs macs;
    if(furi_hal_otp_mac_read(&macs)) {
        for(size_t i = 0; i < FuriHalOtpMacCount; i++) {
            const uint8_t* m = macs.mac[i];
            printf("MAC %-12s %02x:%02x:%02x:%02x:%02x:%02x\r\n", furi_hal_otp_mac_name(i), m[0], m[1], m[2], m[3], m[4], m[5]);
        }
    } else {
        printf("MAC: not written\r\n");
    }
}

static bool cli_command_otp_read_u16(FuriString* args, FuriString* word, uint16_t* value) {
    return args_read_string_and_trim(args, word) && strint_to_uint16(furi_string_get_cstr(word), NULL, value, 10) == StrintParseNoError;
}

static bool cli_command_otp_name_ok(FuriString* name) {
    size_t len = furi_string_size(name);
    if(len == 0 || len >= FURI_HAL_OTP_DEVICE_NAME_SIZE) return false;
    for(size_t i = 0; i < len; i++) {
        char c = furi_string_get_char(name, i);
        if(!isalnum((unsigned char)c) && c != '.' && c != '_' && c != '-') return false;
    }
    return true;
}

static void cli_command_otp_device(FuriString* args) {
    FuriString* word = furi_string_alloc();
    FuriHalOtpDevice device = {0};

    do {
        if(!args_read_string_and_trim(args, word) || !cli_command_otp_name_ok(word)) {
            printf("Invalid NAME\r\n");
            break;
        }
        strlcpy(device.name, furi_string_get_cstr(word), sizeof(device.name));

        if(!cli_command_otp_read_u16(args, word, &device.version) || !cli_command_otp_read_u16(args, word, &device.target) ||
           !cli_command_otp_read_u16(args, word, &device.body) || !cli_command_otp_read_u16(args, word, &device.connect)) {
            printf("VERSION, TARGET, BODY and CONNECT are numbers 0..65535\r\n");
            break;
        }
        if(!args_read_string_and_trim(args, word) || strint_to_uint64(furi_string_get_cstr(word), NULL, &device.timestamp, 10) != StrintParseNoError) {
            printf("Invalid DATE\r\n");
            break;
        }
        if(furi_string_size(args)) {
            printf("Too many arguments\r\n");
            break;
        }

        FuriHalOtpError error = furi_hal_otp_device_write(&device);
        printf("Device: %s\r\n", error == FuriHalOtpErrorNone ? "written and locked, USB white label too" : furi_hal_otp_error_to_string(error));
    } while(0);

    furi_string_free(word);
}

static bool cli_command_otp_parse_mac(const char* str, uint8_t mac[FURI_HAL_OTP_MAC_SIZE]) {
    // aa:bb:cc:dd:ee:ff
    if(strlen(str) != FURI_HAL_OTP_MAC_SIZE * 3 - 1) return false;
    bool zero = true;
    for(size_t i = 0; i < FURI_HAL_OTP_MAC_SIZE; i++) {
        if(i > 0 && str[i * 3 - 1] != ':') return false;
        if(!args_char_to_hex(str[i * 3], str[i * 3 + 1], &mac[i])) return false;
        if(mac[i]) zero = false;
    }
    // An interface address: not all zero, not multicast
    return !zero && !(mac[0] & 0x01);
}

static void cli_command_otp_mac(FuriString* args) {
    FuriString* word = furi_string_alloc();
    FuriHalOtpMacs macs = {0};
    bool ok = true;

    for(size_t i = 0; i < FuriHalOtpMacCount && ok; i++) {
        ok = args_read_string_and_trim(args, word) && cli_command_otp_parse_mac(furi_string_get_cstr(word), macs.mac[i]);
        if(!ok) printf("Invalid MAC for %s\r\n", furi_hal_otp_mac_name(i));
    }
    if(ok && furi_string_size(args)) {
        printf("Too many arguments\r\n");
        ok = false;
    }
    if(ok) {
        FuriHalOtpError error = furi_hal_otp_mac_write(&macs);
        printf("MAC: %s\r\n", error == FuriHalOtpErrorNone ? "written and locked" : furi_hal_otp_error_to_string(error));
    }

    furi_string_free(word);
}

void cli_command_otp(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    UNUSED(context);
    FuriString* action = furi_string_alloc();

    if(!args_read_string_and_trim(args, action)) {
        cli_command_otp_help();
    } else if(furi_string_cmp(action, "info") == 0) {
        cli_command_otp_info();
    } else if(furi_string_cmp(action, "device") == 0) {
        cli_command_otp_device(args);
    } else if(furi_string_cmp(action, "mac") == 0) {
        cli_command_otp_mac(args);
    } else {
        cli_command_otp_help();
    }

    furi_string_free(action);
}

static void cli_command_nvm_help(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    UNUSED(args);
    UNUSED(context);
    printf(
        "nvm list\r\n"
        "nvm get <type:i|u|b|s> <key>\r\n"
        "nvm wipe\r\n"
        "");
}

static bool cli_command_nvm_parse_args(FuriString* args, char* type, FuriString* key) {
    FuriString* type_str = furi_string_alloc();
    bool result = false;
    do {
        if(!args_read_string_and_trim(args, type_str)) {
            break;
        }
        if(!args_read_string_and_trim(args, key)) {
            break;
        }
        if(furi_string_size(type_str) != 1) {
            break;
        }
        *type = furi_string_get_char(type_str, 0);
        result = true;
    } while(0);
    furi_string_free(type_str);
    return result;
}

static bool cli_command_nvm_list_callback(const char* key, void* context) {
    PipeSide* pipe = context;
    printf("%s\r\n", key);
    return !cli_is_pipe_broken_or_is_etx_next_char(pipe);
}

void cli_command_nvm(PipeSide* pipe, FuriString* args, void* context) {
    FuriString* action = furi_string_alloc();
    do {
        if(!args_read_string_and_trim(args, action)) {
            cli_command_nvm_help(pipe, args, context);
            break;
        }

        if(furi_string_cmp(action, "list") == 0) {
            FuriHalNvmStorage res = furi_hal_nvm_list_keys(cli_command_nvm_list_callback, pipe);
            if(res == FuriHalNvmStorageError) {
                printf("NVM error\r\n");
            }
        } else if(furi_string_cmp(action, "get") == 0) {
            FuriString* key = furi_string_alloc();
            char type;
            if(!cli_command_nvm_parse_args(args, &type, key)) {
                cli_command_nvm_help(pipe, args, context);
                furi_string_free(key);
                break;
            }

            FuriHalNvmStorage res = FuriHalNvmStorageError;
            if(type == 'i') {
                int32_t value;
                res = furi_hal_nvm_get_int32(furi_string_get_cstr(key), &value);
                if(res == FuriHalNvmStorageOK) printf("%s: %ld\r\n", furi_string_get_cstr(key), value);
            } else if(type == 'u') {
                uint32_t value;
                res = furi_hal_nvm_get_uint32(furi_string_get_cstr(key), &value);
                if(res == FuriHalNvmStorageOK) printf("%s: %lu\r\n", furi_string_get_cstr(key), value);
            } else if(type == 'b') {
                bool value;
                res = furi_hal_nvm_get_bool(furi_string_get_cstr(key), &value);
                if(res == FuriHalNvmStorageOK) printf("%s: %s\r\n", furi_string_get_cstr(key), value ? "true" : "false");
            } else if(type == 's') {
                FuriString* value = furi_string_alloc();
                res = furi_hal_nvm_get_str(furi_string_get_cstr(key), value);
                if(res == FuriHalNvmStorageOK) printf("%s: %s\r\n", furi_string_get_cstr(key), furi_string_get_cstr(value));
                furi_string_free(value);
            } else {
                cli_command_nvm_help(pipe, args, context);
                furi_string_free(key);
                break;
            }

            if(res == FuriHalNvmStorageItemNotFound) {
                printf("Key %s is missing\r\n", furi_string_get_cstr(key));
            } else if(res == FuriHalNvmStorageError) {
                printf("NVM error\r\n");
            }
            furi_string_free(key);
        } else if(furi_string_cmp(action, "wipe") == 0) {
            furi_hal_nvm_wipe();
            printf("Done\r\n");
            break;
        } else {
            cli_command_nvm_help(pipe, args, context);
        }
    } while(0);
    furi_string_free(action);
}
