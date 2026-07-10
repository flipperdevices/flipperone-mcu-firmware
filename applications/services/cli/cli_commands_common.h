#pragma once

#include <containers/pipe.h>

void cli_command_uptime(PipeSide* pipe, FuriString* args, void* context);
void cli_command_log(PipeSide* pipe, FuriString* args, void* context);
void cli_command_top(PipeSide* pipe, FuriString* args, void* context);
void cli_command_free(PipeSide* pipe, FuriString* args, void* context);
void cli_command_free_blocks(PipeSide* pipe, FuriString* args, void* context);
void cli_command_i2c(PipeSide* pipe, FuriString* args, void* context);
void cli_command_expander_ext(PipeSide* pipe, FuriString* args, void* context);
void cli_command_clock_out(PipeSide* pipe, FuriString* args, void* context);
void cli_command_otp(PipeSide* pipe, FuriString* args, void* context);
void cli_command_dmesg(PipeSide* pipe, FuriString* args, void* context);
void cli_log_history_init(void);