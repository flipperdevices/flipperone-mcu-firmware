#pragma once

typedef struct UartEchoApp UartEchoApp;

#ifdef __cplusplus
extern "C" {
#endif

UartEchoApp* uart_echo_app_start(void);
void uart_echo_app_stop(UartEchoApp* app);

#ifdef __cplusplus
}
#endif
