#include "dmesg_cli.h"
#include "dmesg_app.h"

#include <cli/args.h>

#define TAG               "DmesgCli"
#define DMESG_BUFFER_SIZE 1024

static void dmesg_cli_command_dmesg_help(void) {
    printf("Usage:\r\n");
    printf(" dmesg\t - show the last log line\r\n");
}

static void dmesg_cli_show_log(PipeSide* pipe, FuriString* args) {
    UNUSED(pipe);
    UNUSED(args);

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
}

void dmesg_cli(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    UNUSED(context);

    FuriString* cmd;
    cmd = furi_string_alloc();

    do {
        if(!args_read_string_and_trim(args, cmd)) {
            dmesg_cli_show_log(pipe, args);
            break;
        }

        dmesg_cli_command_dmesg_help();
    } while(false);

    furi_string_free(cmd);
}
