/**
 * @file cli_command_min.h
 * Bare minimum for internal CLI commands including redundant opaque types
 * defined elsewhere. This file is needed for `applications.h`.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CliCommandFlagDefault = 0, /**< Default */
    CliCommandFlagParallelSafe = (1 << 0), /**< Safe to run in parallel with other apps */
    CliCommandFlagInsomniaSafe = (1 << 1), /**< Safe to run with insomnia mode on */
    CliCommandFlagDontAttachStdio = (1 << 2), /**< Do no attach I/O pipe to thread stdio */
    CliCommandFlagUseShellThread =
        (1
         << 3), /**< Don't start a separate thread to run the command in. Incompatible with DontAttachStdio */

    // internal flags (do not set them yourselves!)

    CliCommandFlagExternal = (1 << 4), /**< The command comes from a .fal file */
    CliCommandFlagExclusive =
        (1 << 5), /**< Only one instance of the command can be run at a time */
} CliCommandFlag;

typedef struct PipeSide PipeSide;
typedef struct FuriString FuriString;

/** 
 * @brief CLI command execution callback pointer
 * 
 * This callback will be called from a separate thread spawned just for your
 * command. The pipe will be installed as the thread's stdio, so you can use
 * `printf`, `getchar` and other standard functions to communicate with the
 * user.
 * 
 * @param [in] pipe     Pipe that can be used to send and receive data. If
 *                      `CliCommandFlagDontAttachStdio` was not set, you can
 *                      also use standard C functions (printf, getc, etc.) to
 *                      access this pipe.
 * @param [in] args     String with what was passed after the command
 * @param [in] context  Whatever you provided to `cli_add_command`
 */
typedef void (*CliCommandExecuteCallback)(PipeSide* pipe, FuriString* args, void* context);

#ifdef __cplusplus
}
#endif
