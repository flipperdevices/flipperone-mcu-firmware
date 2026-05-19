// #include <core/thread.h>
// #include <core/thread_list.h>
#include <furi_hal.h>
// #include <furi_hal_info.h>
// #include <task_control_block.h>
// #include <time.h>
#include <cli/args.h>
#include <cli/cli_registry.h>
//#include <cli/cli_ansi.h>
#include <cli/cli_commands.h>
#include <applications.h>

#include "cli_command_gpio.h"
#include "cli_commands_common.h"


static void cli_commands_init(CliRegistry* registry) {
    cli_registry_add_command(registry, "uptime", CliCommandFlagParallelSafe, cli_command_uptime, NULL);
    cli_registry_add_command(registry, "log", CliCommandFlagParallelSafe, cli_command_log, NULL);
    cli_registry_add_command(registry, "top", CliCommandFlagParallelSafe, cli_command_top, NULL);
    cli_registry_add_command(registry, "free", CliCommandFlagParallelSafe, cli_command_free, NULL);
    cli_registry_add_command(registry, "free_blocks", CliCommandFlagParallelSafe, cli_command_free_blocks, NULL);
    cli_registry_add_command(registry, "i2c", CliCommandFlagParallelSafe, cli_command_i2c, NULL);
    cli_registry_add_command(registry, "expander_ext", CliCommandFlagParallelSafe, cli_command_expander_ext, NULL);
    cli_registry_add_command(registry, "clock_out", CliCommandFlagParallelSafe, cli_command_clock_out, NULL);
    cli_registry_add_command(registry, "gpio", CliCommandFlagParallelSafe, cli_command_gpio, NULL);
    if(!furi_hal_otp_usb_white_label_valid()) {
        cli_registry_add_command(registry, "otp", CliCommandFlagParallelSafe, cli_command_otp, NULL);
    }

    // commands from `.fam`s
    for(size_t i = 0; i < FLIPPER_CLI_COMMANDS_COUNT; i++) {
        const FlipperInternalCommandApplication* command = &FLIPPER_CLI_COMMANDS[i];
        cli_registry_add_command_ex(
            registry, command->name, command->flags, command->callback, NULL, command->stack_size);
    }
}

int32_t cli_on_system_start(void* p) {
    UNUSED(p);
    CliRegistry* registry = cli_registry_alloc();
    cli_commands_init(registry);
    furi_record_create(RECORD_CLI, registry);
    return 0;
}
