/**
 * @file cli_registry_i.h
 * Internal API for getting commands registered with the CLI
 */

#pragma once

#include <furi.h>
#include <m-dict.h>
#include "cli_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CLI_BUILTIN_COMMAND_STACK_SIZE (4 * 1024U)

typedef struct {
    void* context; //<! Context passed to callbacks
    CliCommandExecuteCallback execute_callback; //<! Callback for command execution
    CliCommandFlag flags;
    size_t stack_depth;
    FuriSemaphore* run_semaphore;
} CliRegistryCommand;

void cli_registry_command_clear(CliRegistryCommand cmd);

#define CLI_REGISTRY_COMMAND_CLEAR(a)                             \
    do {                                                          \
        if(a.run_semaphore) furi_semaphore_free(a.run_semaphore); \
    } while(0)
#define CLI_REGISTRY_COMMAND_OPTLIST M_OPEXTEND(M_POD_OPLIST, CLEAR(CLI_REGISTRY_COMMAND_CLEAR))
DICT_DEF2(
    CliCommandDict,
    FuriString*,
    FURI_STRING_OPLIST,
    CliRegistryCommand,
    CLI_REGISTRY_COMMAND_OPTLIST);
#define M_OPL_CliCommandDict_t() DICT_OPLIST(CliCommandDict, FURI_STRING_OPLIST, M_POD_OPLIST)

bool cli_registry_get_command(
    CliRegistry* registry,
    FuriString* command,
    CliRegistryCommand* result);

void cli_registry_lock(CliRegistry* registry);

void cli_registry_unlock(CliRegistry* registry);

/**
 * @warning Surround calls to this function with `cli_registry_[un]lock`
 */
CliCommandDict_t* cli_registry_get_commands(CliRegistry* registry);

#ifdef __cplusplus
}
#endif
