#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define RECORD_CLI_UART "cli_uart"

typedef struct CliUart CliUart;

void cli_uart_enable(CliUart* cli_uart);
void cli_uart_disable(CliUart* cli_uart);

#ifdef __cplusplus
}
#endif
