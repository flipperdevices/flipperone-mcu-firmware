#include "uart_echo_cli.h"
#include "uart_echo.h"

#include <cli/args.h>

UartEchoApp* uart_echo_app_helper = NULL;

static void uart_echo_cli_command_print_usage() {
    printf(
        "Usage: uart_echo <COMMAND>\r\n"
        "Where <COMMAND> is:\r\n"
        "\tstart \tStart the UART echo application\r\n"
        "\tstop \tStop the UART echo application\r\n");
}

void uart_echo_cli(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(context);
    UNUSED(pipe);

    FuriString* cmd;
    cmd = furi_string_alloc();

    do {
        if(!args_read_string_and_trim(args, cmd)) {
            uart_echo_cli_command_print_usage();
            break;
        }

        if(furi_string_cmp_str(cmd, "start") == 0) {
            uart_echo_app_helper = uart_echo_app_start();
            break;
        }

        if(furi_string_cmp_str(cmd, "stop") == 0) {
            uart_echo_app_stop(uart_echo_app_helper);
            uart_echo_app_helper = NULL;
            break;
        }
    
        uart_echo_cli_command_print_usage();
    } while(false);

    furi_string_free(cmd);
}
