#pragma once

#include <containers/pipe.h>

void uart_echo_cli(PipeSide* pipe, FuriString* args, void* context);
