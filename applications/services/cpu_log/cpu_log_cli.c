#include "cpu_log.h"
#include <containers/pipe.h>
#include <cli/cli_command.h>
#include <cli/cli_registry.h>
#include <cli/cli_commands.h>

static void cpu_log_cli_tx_callback(const uint8_t* data, size_t size, void* context) {
    PipeSide* pipe = context;
    pipe_send(pipe, data, size);
}

void cpu_log_cli(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(context);

    if(furi_string_size(args) == 0) {
        printf("\r\nPress CTRL+C to stop...\r\n\r\n");

        CpuLogSrv* app = furi_record_open(RECORD_CPU_LOG);
        CpuLogHandler handler = {.callback = cpu_log_cli_tx_callback, .context = pipe};
        cpu_log_add_handler(app, &handler);

        while(!cli_is_pipe_broken_or_is_etx_next_char(pipe)) {
            furi_delay_ms(100);
        }

        cpu_log_remove_handler(app, &handler);
        furi_record_close(RECORD_CPU_LOG);
    } else if(furi_string_cmp_str(args, "clear") == 0) {
        CpuLogSrv* app = furi_record_open(RECORD_CPU_LOG);
        cpu_log_clear(app);
        furi_record_close(RECORD_CPU_LOG);
    } else {
        cli_print_usage("cpu_log", "[clear]", furi_string_get_cstr(args));
    }
}
