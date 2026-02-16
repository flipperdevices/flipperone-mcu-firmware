#include "cli_commands.h"

#include <core/thread.h>
#include <core/thread_list.h>
#include <furi_hal.h>
#include <task_control_block.h>
#include <time.h>
#include <args.h>
#include "furi_bsp.h"
#include <status_lights/status_lights.h>

static void cli_command_help(Cli* cli, FuriString* args, void* context) {
    UNUSED(args);
    UNUSED(context);
    printf("Commands available:");

    // Command count
    const size_t commands_count = CliCommandTree_size(cli->commands);
    const size_t commands_count_mid = commands_count / 2 + commands_count % 2;

    // Use 2 iterators from start and middle to show 2 columns
    CliCommandTree_it_t it_left;
    CliCommandTree_it(it_left, cli->commands);
    CliCommandTree_it_t it_right;
    CliCommandTree_it(it_right, cli->commands);
    for(size_t i = 0; i < commands_count_mid; i++)
        CliCommandTree_next(it_right);

    // Iterate throw tree
    for(size_t i = 0; i < commands_count_mid; i++) {
        printf("\r\n");
        // Left Column
        if(!CliCommandTree_end_p(it_left)) {
            printf("%-30s", furi_string_get_cstr(*CliCommandTree_ref(it_left)->key_ptr));
            CliCommandTree_next(it_left);
        }
        // Right Column
        if(!CliCommandTree_end_p(it_right)) {
            printf("%s", furi_string_get_cstr(*CliCommandTree_ref(it_right)->key_ptr));
            CliCommandTree_next(it_right);
        }
    };

    if(furi_string_size(args) > 0) {
        cli_nl(cli);
        printf("`");
        printf("%s", furi_string_get_cstr(args));
        printf("` command not found");
    }
}

static void cli_command_uptime(Cli* cli, FuriString* args, void* context) {
    UNUSED(cli);
    UNUSED(args);
    UNUSED(context);
    uint32_t uptime = furi_get_tick() / furi_kernel_get_tick_frequency();
    printf("Uptime: %luh%lum%lus", uptime / 60 / 60, uptime / 60 % 60, uptime % 60);
}

#define CLI_COMMAND_LOG_RING_SIZE   2048
#define CLI_COMMAND_LOG_BUFFER_SIZE 64

static void cli_command_log_tx_callback(const uint8_t* buffer, size_t size, void* context) {
    furi_stream_buffer_send(context, buffer, size, 0);
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

static void cli_command_log(Cli* cli, FuriString* args, void* context) {
    UNUSED(context);
    FuriStreamBuffer* ring = furi_stream_buffer_alloc(CLI_COMMAND_LOG_RING_SIZE, 1);
    uint8_t buffer[CLI_COMMAND_LOG_BUFFER_SIZE];
    FuriLogLevel previous_level = furi_log_get_level();
    bool restore_log_level = false;

    if(furi_string_size(args) > 0) {
        if(!cli_command_log_level_set_from_string(args)) {
            furi_stream_buffer_free(ring);
            return;
        }
        restore_log_level = true;
    }

    const char* current_level;
    furi_log_level_to_string(furi_log_get_level(), &current_level);
    printf("Current log level: %s\r\n", current_level);

    FuriLogHandler log_handler = {
        .callback = cli_command_log_tx_callback,
        .context = ring,
    };

    furi_log_add_handler(log_handler);

    printf("Use <log ?> to list available log levels\r\n");
    printf("Press CTRL+C to stop...\r\n");
    while(!cli_cmd_interrupt_received(cli)) {
        size_t ret = furi_stream_buffer_receive(ring, buffer, CLI_COMMAND_LOG_BUFFER_SIZE, 50);
        cli_write(cli, buffer, ret);
    }

    furi_log_remove_handler(log_handler);

    if(restore_log_level) {
        // There will be strange behaviour if log level is set from settings while log command is running
        furi_log_set_level(previous_level);
    }

    furi_stream_buffer_free(ring);
}

static void cli_command_top(Cli* cli, FuriString* args, void* context) {
    UNUSED(cli);
    UNUSED(context);

    int interval = 1000;
    args_read_int_and_trim(args, &interval);

    FuriThreadList* thread_list = furi_thread_list_alloc();
    while(!cli_cmd_interrupt_received(cli)) {
        uint32_t tick = furi_get_tick();
        furi_thread_enumerate(thread_list);

        if(interval) printf("\e[2J\e[0;0f"); // Clear display and return to 0

        uint32_t uptime = tick / furi_kernel_get_tick_frequency();
        printf(
            "Threads: %zu, ISR Time: %0.2f%%, Uptime: %luh%lum%lus\r\n",
            furi_thread_list_size(thread_list),
            (double)furi_thread_list_get_isr_time(thread_list),
            uptime / 60 / 60,
            uptime / 60 % 60,
            uptime % 60);

        printf(
            "Heap: total %zu, free %zu, minimum %zu, max block %zu\r\n\r\n",
            memmgr_get_total_heap(),
            memmgr_get_free_heap(),
            memmgr_get_minimum_free_heap(),
            memmgr_heap_get_max_free_block());

        printf("%-25s %-20s %-10s %5s %12s %6s %10s %7s %5s\r\n", "AppID", "Name", "State", "Prio", "Stack start", "Stack", "Stack Min", "Heap", "CPU");

        for(size_t i = 0; i < furi_thread_list_size(thread_list); i++) {
            const FuriThreadListItem* item = furi_thread_list_get_at(thread_list, i);
            printf(
                "%-25s %-20s %-10s %5d   0x%08lx %6lu %10lu %7zu %5.1f\r\n",
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

        if(interval > 0) {
            furi_delay_ms(interval);
        } else {
            break;
        }
    }
    furi_thread_list_free(thread_list);
}

static void cli_command_free(Cli* cli, FuriString* args, void* context) {
    UNUSED(cli);
    UNUSED(args);
    UNUSED(context);

    printf("Free heap size: %zu\r\n", memmgr_get_free_heap());
    printf("Total heap size: %zu\r\n", memmgr_get_total_heap());
    printf("Minimum heap size: %zu\r\n", memmgr_get_minimum_free_heap());
    printf("Maximum heap block: %zu\r\n", memmgr_heap_get_max_free_block());

    printf("Pool free: %zu\r\n", memmgr_pool_get_free());
    printf("Maximum pool block: %zu\r\n", memmgr_pool_get_max_block());
}

static void cli_command_free_blocks(Cli* cli, FuriString* args, void* context) {
    UNUSED(cli);
    UNUSED(args);
    UNUSED(context);

    memmgr_heap_printf_free_blocks();
}

static void cli_command_expander_ext_help(Cli* cli, FuriString* args, void* context) {
    UNUSED(cli);
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

static void cli_command_expander_ext(Cli* cli, FuriString* args, void* context) {
    UNUSED(cli);
    UNUSED(context);

    if(furi_string_size(args) < 2) {
        cli_command_expander_ext_help(cli, args, context);
        return;
    }

    int expander_gpio_out_number = 0;
    if(!args_read_int_and_trim(args, &expander_gpio_out_number)) {
        cli_command_expander_ext_help(cli, args, context);
        return;
    }
    if(expander_gpio_out_number < 0 || expander_gpio_out_number > 7) {
        cli_command_expander_ext_help(cli, args, context);
        return;
    }

    int expander_gpio_out_value = 0;
    if(!args_read_int_and_trim(args, &expander_gpio_out_value)) {
        cli_command_expander_ext_help(cli, args, context);
        return;
    }
    if(expander_gpio_out_value != 0 && expander_gpio_out_value != 1) {
        cli_command_expander_ext_help(cli, args, context);
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

static void cli_command_set_led_help(Cli* cli, FuriString* args, void* context) {
    UNUSED(cli);
    UNUSED(args);
    UNUSED(context);
    printf(
        "Usage: set_led <LED_TYPE> <COLOR>\r\n"
        "Where <LED_TYPE> is:\r\n"
        "\tNet \t\t0\r\n"
        "\tWiFi \t\t1\r\n"
        "\tEth2 \t\t2\r\n"
        "\tEth1 \t\t3\r\n"
        "\tPower \t\t4\r\n"
        "\tBatteryOutline\t5\r\n"
        "\tBatteryWatt1\t6\r\n"
        "\tBatteryWatt2\t7\r\n"
        "\tBatteryWatt3\t8\r\n"
        "\tBatteryWatt4\t9\r\n"
        "\tUsbCharging \t10\r\n"
        "\tUsbWatt1 \t11\r\n"
        "\tUsbWatt2 \t12\r\n"
        "\tUsbWatt3 \t13\r\n"
        "\tUsbWatt4 \t14\r\n"
        "\tBatteryCenter\t15\r\n"
        "\tLineAllOff \t16\r\n");
    printf(
        "Where <COLOR> is:\r\n"
        "\tred \t\t0\r\n"
        "\tgreen \t\t1\r\n"
        "\tblue \t\t2\r\n"
        "\tyellow \t\t3\r\n"
        "\torange \t\t4\r\n"
        "\tlight_blue \t5\r\n"
        "\tblack \t\t6\r\n");
}

StatusLightsType cli_status_lights_types[] = {
    StatusLightsTypeNet,
    StatusLightsTypeWiFi,
    StatusLightsTypeEth2,
    StatusLightsTypeEth1,
    StatusLightsTypePower,
    StatusLightsTypeBatteryOutline,
    StatusLightsTypeBatteryWatt1,
    StatusLightsTypeBatteryWatt2,
    StatusLightsTypeBatteryWatt3,
    StatusLightsTypeBatteryWatt4,
    StatusLightsTypeUsbCharging,
    StatusLightsTypeUsbWatt1,
    StatusLightsTypeUsbWatt2,
    StatusLightsTypeUsbWatt3,
    StatusLightsTypeUsbWatt4,
    StatusLightsTypeBatteryCenter,
    StatusLightsTypeLineAllOff,
};

StatusLightsColor cli_status_lights_colors[] = {
    STATUS_LIGHTS_COLOR_RED,
    STATUS_LIGHTS_COLOR_GREEN,
    STATUS_LIGHTS_COLOR_BLUE,
    STATUS_LIGHTS_COLOR_YELLOW,
    STATUS_LIGHTS_COLOR_ORANGE,
    STATUS_LIGHTS_COLOR_LIGHT_BLUE,
    STATUS_LIGHTS_COLOR_BLACK,
};

static void cli_command_set_led(Cli* cli, FuriString* args, void* context) {
    UNUSED(cli);
    UNUSED(context);

    if(furi_string_size(args) < 1) {
        cli_command_set_led_help(cli, args, context);
        return;
    }

    int led_type = 0;
    int color = 0;
    if(!args_read_int_and_trim(args, &led_type)) {
        cli_command_set_led_help(cli, args, context);
        return;
    }
    if(led_type < 0 || led_type >= sizeof(cli_status_lights_types) / sizeof(StatusLightsType)) {
        cli_command_set_led_help(cli, args, context);
        return;
    }

    if(furi_string_size(args) < 1 && cli_status_lights_types[led_type] != StatusLightsTypeLineAllOff) { 
        cli_command_set_led_help(cli, args, context);
        return;
    }

    if(cli_status_lights_types[led_type] != StatusLightsTypeLineAllOff) {
        if(!args_read_int_and_trim(args, &color)) {
            cli_command_set_led_help(cli, args, context);
            return;
        }
        if(color < 0 || color >= sizeof(cli_status_lights_colors) / sizeof(StatusLightsColor)) {
            cli_command_set_led_help(cli, args, context);
            return;
        }
    }

    StatusLights* status_lights = furi_record_open(RECORD_STATUS_LIGHTS);
    status_lights_notification(status_lights, cli_status_lights_types[led_type], cli_status_lights_colors[color]);
    furi_record_close(RECORD_STATUS_LIGHTS);
}

void cli_commands_init(Cli* cli) {
    cli_add_command(cli, "?", CliCommandFlagParallelSafe, cli_command_help, NULL);
    cli_add_command(cli, "help", CliCommandFlagParallelSafe, cli_command_help, NULL);

    cli_add_command(cli, "uptime", CliCommandFlagParallelSafe, cli_command_uptime, NULL);
    cli_add_command(cli, "log", CliCommandFlagParallelSafe, cli_command_log, NULL);
    cli_add_command(cli, "top", CliCommandFlagParallelSafe, cli_command_top, NULL);
    cli_add_command(cli, "free", CliCommandFlagParallelSafe, cli_command_free, NULL);
    cli_add_command(cli, "free_blocks", CliCommandFlagParallelSafe, cli_command_free_blocks, NULL);
    cli_add_command(cli, "expander_ext", CliCommandFlagParallelSafe, cli_command_expander_ext, NULL);
    cli_add_command(cli, "set_led", CliCommandFlagParallelSafe, cli_command_set_led, NULL);
}
