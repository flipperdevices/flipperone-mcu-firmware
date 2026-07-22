#include "cli_command_device_info.h"
#include <furi_hal_info.h>

#include <cli/args.h>

#define DEVICE_INFO_NAME "Flipper One"

static void cli_command_device_info_callback(const char* key, const char* value, bool last, void* context) {
    UNUSED(last);
    UNUSED(context);
    printf("%-30s: %s\r\n", key, value);
}

static void cli_command_device_info_print_name() {
    FuriString* name = furi_string_alloc();
    furi_string_set_str(name, DEVICE_INFO_NAME);
    cli_command_device_info_callback("name", furi_string_get_cstr(name), false, NULL);
    furi_string_free(name);
}

void cli_command_device_info(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    UNUSED(args);
    UNUSED(context);

    cli_command_device_info_print_name();
    furi_hal_info_get(cli_command_device_info_callback, '_', NULL);
}
