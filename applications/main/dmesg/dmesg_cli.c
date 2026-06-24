#include "dmesg_cli.h"
#include "dmesg_app.h"

#include <cli/args.h>
#include <cli/cli_commands_common.h>

#define TAG               "DmesgCli"
#define DMESG_BUFFER_SIZE 1024

static void dmesg_cli_command_dmesg_help(void) {
    printf("Usage:\r\n");

    printf(" dmesg [w] [log level]\t - show the last log line, if \"w\" is provided, wait for new log lines\r\n");
    printf(" \t\t\tLog level can be one of: default, none, error, warn, info, debug, trace\r\n");
}

static void dmesg_cli_show_log(PipeSide* pipe, FuriString* args) {
    bool wait_for_new_logs = false;

    if(furi_string_size(args) > 0) {
        FuriString* cmd;
        cmd = furi_string_alloc();

        args_read_string_and_trim(args, cmd);
        if(furi_string_cmp_str(cmd, "w") == 0) {
            wait_for_new_logs = true;
        } else {
            dmesg_cli_command_dmesg_help();
            furi_string_free(cmd);
            return;
        }
        furi_string_free(cmd);
    }

    DmesgApp* instance = furi_record_open(RECORD_DMESG_APP);

    size_t size = DMESG_BUFFER_SIZE;
    uint8_t buffer[DMESG_BUFFER_SIZE];

    dmesg_app_read_acquire(instance);
    dmesg_app_update_read_index(instance);

    while(dmesg_app_get_log_data(instance, buffer, &size)) {
        if(size != 0) {
            printf("%.*s", (int)size, buffer);
        }
        size = DMESG_BUFFER_SIZE;
    }
    dmesg_app_restore_read_index(instance);
    dmesg_app_read_release(instance);

    furi_record_close(RECORD_DMESG_APP);

    if(wait_for_new_logs) {
        cli_command_log(pipe, args, NULL);
    }
}

void dmesg_cli(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(context);
    dmesg_cli_show_log(pipe, args);
}
