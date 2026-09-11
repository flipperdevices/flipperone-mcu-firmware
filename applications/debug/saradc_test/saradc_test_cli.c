#include "saradc_test_cli.h"

#include <cli/args.h>
#include <toolbox/strint.h>
#include <cli/cli_ansi.h>
#include <cli/cli_command.h>
#include <furi_bsp_saradc.h>

typedef struct {
    const char* name;
    const char* arg_spec;
    const char* description;
    bool (*execute)(PipeSide*, FuriString*);
} SaradcCmd;

static bool saradc_test_cli_set_id(PipeSide* pipe, FuriString* args) {
    UNUSED(pipe);
    bool ret = false;
    do {
        uint8_t id;

        if(!furi_string_size(args)) {
            break;
        }

        const char* args_cstr = furi_string_get_cstr(args);
        StrintParseError parse_err = StrintParseNoError;
        parse_err |= strint_to_uint8(args_cstr, &args_cstr, &id, 10);
        if(parse_err || id > FuriBspSaradcIdMax || id == 0) {
            printf(ANSI_FG_RED "Invalid SARADC ID:" ANSI_RESET " %s\r\n", furi_string_get_cstr(args));
            break;
        }
        furi_bsp_saradc_set_id((FuriBspSaradcId)(id - 1)); // Adjust for 0-based index);
        printf(ANSI_FG_GREEN "SARADC ID set to" ANSI_RESET " %d\r\n", id);
        ret = true;

    } while(false);

    return ret;
}

static bool saradc_test_cli_get_id(PipeSide* pipe, FuriString* args) {
    UNUSED(pipe);
    UNUSED(args);
    FuriBspSaradcId id = furi_bsp_saradc_get_id();
    printf(ANSI_FG_GREEN "Current SARADC ID:" ANSI_RESET " %d\r\n", id + 1); // Adjust for 0-based index
    return true;
}

static const SaradcCmd saradc_test_cmds[] = {
    {"set_id", "<id>", "Set SARADC ID (1..%d)", saradc_test_cli_set_id},
    {"get_id", "", "Get SARADC ID", saradc_test_cli_get_id},
};

static void saradc_test_command_cli_print_usage(void) {
    printf("Usage:\r\nSaradc <cmd>\r\nCmd list:\r\n");
    for(size_t i = 0; i < COUNT_OF(saradc_test_cmds); i++) {
        const SaradcCmd* c = &saradc_test_cmds[i];
        printf("\t%s %s - ", c->name, c->arg_spec);
        printf(c->description, (int)FuriBspSaradcIdMax);
        printf("\r\n");
    }
}

void saradc_test_command_cli(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(context);
    FuriString* cmd = furi_string_alloc();
    bool handled = false;

    if(args_read_string_and_trim(args, cmd)) {
        const char* cmd_str = furi_string_get_cstr(cmd);
        for(size_t i = 0; i < COUNT_OF(saradc_test_cmds); i++) {
            const SaradcCmd* c = &saradc_test_cmds[i];
            if(strcmp(cmd_str, c->name) == 0) {
                if(!c->execute(pipe, args)) {
                    printf("usage: Saradc %s %s\r\n", c->name, c->arg_spec);
                }
                handled = true;
                break;
            }
        }
    }

    if(!handled) {
        saradc_test_command_cli_print_usage();
    }

    furi_string_free(cmd);
}
