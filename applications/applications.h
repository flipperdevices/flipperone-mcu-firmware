#pragma once

#include <furi.h>
#include <cli/cli.h>

typedef enum {
    FlipperInternalApplicationFlagDefault = 0,
    FlipperInternalApplicationFlagInsomniaSafe = (1 << 0),
} FlipperInternalApplicationFlag;

typedef struct {
    const FuriThreadCallback app;
    const char* name;
    const char* appid;
    const size_t stack_size;
    const FlipperInternalApplicationFlag flags;
} FlipperInternalApplication;

typedef struct {
    const char* name;
    const char* path;
} FlipperExternalApplication;

typedef void (*FlipperInternalOnStartHook)(void);

typedef struct {
    const CliCallback callback;
    const char* name;
    const size_t stack_size;
    const CliCommandFlag flags;
} FlipperInternalCommandApplication;

extern const char* FLIPPER_AUTORUN_APP_NAME;

/* Services list
 * Spawned on startup
 */
extern const FlipperInternalApplication FLIPPER_SERVICES[];
extern const size_t FLIPPER_SERVICES_COUNT;

/* Apps list
 * Spawned by loader
 */
extern const FlipperInternalApplication FLIPPER_APPS[];
extern const size_t FLIPPER_APPS_COUNT;

/* Internal CLI commands
 * Added to main CLI registry by CLI startup hook
 */
extern const FlipperInternalCommandApplication FLIPPER_CLI_COMMANDS[];
extern const size_t FLIPPER_CLI_COMMANDS_COUNT;