#include "desktop_cli.h"
#include "desktop.h"

#include <cli/args.h>
#include <toolbox/strint.h>
#include <cli/cli_ansi.h>
#include <cli/cli_command.h>
#include <furi_hal.h>


typedef struct {
    const char* name;
    const char* arg_spec;
    const char* description;
    bool (*execute)(PipeSide*, FuriString*);
} DesktopCmd; 


static bool desktop_cli_list_apps(PipeSide* pipe, FuriString* args) {
    UNUSED(pipe);
    UNUSED(args);

    printf("Available apps:\r\n");
    for(size_t i = 0; i < FLIPPER_APPS_COUNT; i++) {
        printf("\t%s (%s)\r\n", FLIPPER_APPS[i].name, FLIPPER_APPS[i].appid);
    }
    return true;
}

static bool desktop_cli_start_app(PipeSide* pipe, FuriString* args) {
    UNUSED(pipe);

    FuriString* app_name = furi_string_alloc();
    const FlipperInternalApplication* target = NULL;

    do {
        if(!args_read_string_and_trim(args, app_name)) {
            printf("usage: desktop start_app <app_name|appid>\r\n");
            break;
        }

        const char* name = furi_string_get_cstr(app_name);
        for(size_t i = 0; i < FLIPPER_APPS_COUNT; i++) {
            if(strcmp(name, FLIPPER_APPS[i].name) == 0 ||
               strcmp(name, FLIPPER_APPS[i].appid) == 0) {
                target = &FLIPPER_APPS[i];
                break;
            }
        }

        if(!target) {
            printf("app not found: %s\r\n", name);
            break;
        }

        if(!desktop_start_app(target)) {
            const char* running = desktop_get_running_app_name();
            printf(
                "failed to start %s: %s is already running\r\n",
                target->appid,
                running ? running : "unknown app");
            break;
        }

        printf("started %s (%s)\r\n", target->name, target->appid);
    } while(false);

    furi_string_free(app_name);
    return true;
}

static bool desktop_cli_stop_app(PipeSide* pipe, FuriString* args) {
    UNUSED(pipe);
    UNUSED(args);

    if(!desktop_stop_app()) {
        printf("no app is running\r\n");
    }
    return true;
}

static const DesktopCmd desktop_cmds[] = {
    {"start_app", "<app_name|appid>", "Start an application by name or appid", desktop_cli_start_app},
    {"stop_app", "", "Request graceful exit of the running app", desktop_cli_stop_app},
    {"list_apps", "", "List available apps", desktop_cli_list_apps},
};

static void desktop_command_cli_print_usage(void) {
    printf("Usage:\r\ndesktop <cmd>\r\nCmd list:\r\n");
    for(size_t i = 0; i < COUNT_OF(desktop_cmds); i++) {
        const DesktopCmd* c = &desktop_cmds[i];
        printf("\t%s %s - %s\r\n", c->name, c->arg_spec, c->description);
    }
}

void desktop_command_cli(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(context);
    FuriString* cmd = furi_string_alloc();
    bool handled = false;

    if(args_read_string_and_trim(args, cmd)) {
        const char* cmd_str = furi_string_get_cstr(cmd);
        for(size_t i = 0; i < COUNT_OF(desktop_cmds); i++) {
            const DesktopCmd* c = &desktop_cmds[i];
            if(strcmp(cmd_str, c->name) == 0) {
                if(!c->execute(pipe, args)) {
                    printf("usage: desktop %s %s\r\n", c->name, c->arg_spec);
                }
                handled = true;
                break;
            }
        }
    }

    if(!handled) {
        desktop_command_cli_print_usage();
    }

    furi_string_free(cmd);
}
