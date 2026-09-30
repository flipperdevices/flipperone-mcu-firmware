#include "hmi_test_cli.h"

#include <pico/stdio.h>

#include <applications.h>
#include <desktop/desktop.h>
#include <debug/touchpad_test_v2/touchpad_test.h>

#include <cli/args.h>
#include <toolbox/strint.h>
#include <cli/cli_ansi.h>
#include <cli/cli_command.h>
#include <cli/cli_status.h>
#include <furi_hal.h>

typedef struct {
    const char* name;
    const char* arg_spec;
    const char* description;
    bool (*execute)(PipeSide*, FuriString*);
} HmiTestCmd;

static const FlipperInternalApplication* hmi_test_find_app_by_appid(const char* appid) {
    for(size_t i = 0; i < FLIPPER_APPS_COUNT; i++) {
        if(strcmp(FLIPPER_APPS[i].appid, appid) == 0) {
            return &FLIPPER_APPS[i];
        }
    }
    return NULL;
}

static void hmi_test_touchpad_event_callback(const void* value, void* ctx) {
    furi_assert(value);
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;
    TouchpadTestEvent event = *(const TouchpadTestEvent*)value;
    furi_message_queue_put(event_queue, &event, FuriWaitForever);
}

static bool hmi_test_cli_start_touch(PipeSide* pipe, FuriString* args) {
    UNUSED(args);

    const char* appid = "touchpad_test_v2";

    const FlipperInternalApplication* target = hmi_test_find_app_by_appid(appid);
    if(!target) {
        printf(ANSI_FG_RED "Application not found: %s" ANSI_RESET "\r\n", appid);
        return true;
    }

    printf("Starting touch test...\r\n");
    if(!desktop_start_app(target)) {
        printf(ANSI_FG_RED "Failed to start touch test application." ANSI_RESET "\r\n");
        printf(CLI_STATUS_ERROR);
        return true;
    }

    // furi_record_open() blocks forever until the record becomes ready - fine
    // if touchpad_test_v2 is merely slow to start, but if it never reaches
    // furi_record_create() (crashed, or exited and tore the record back down
    // before we get here) nothing will ever set that flag and we'd hang for
    // good, with no way for Ctrl+C to break out since the wait loop below
    // isn't even entered yet. Poll furi_record_exists() with a bounded budget
    // instead, so a dead/never-started app fails with a message rather than
    // wedging the CLI thread permanently.
    const int record_wait_budget_ms = 2000;
    int record_waited_ms = 0;
    while(!furi_record_exists(RECORD_TOUCHPAD_TEST) && record_waited_ms < record_wait_budget_ms) {
        furi_delay_ms(20);
        record_waited_ms += 20;
    }
    if(!furi_record_exists(RECORD_TOUCHPAD_TEST)) {
        printf(ANSI_FG_RED "Touch test app never became ready." ANSI_RESET "\r\n");
        printf(CLI_STATUS_ERROR);
        return true;
    }

    // furi_pubsub_publish() runs subscriber callbacks synchronously on the
    // publisher's (touchpad_test_v2's) thread, not ours - so the callback
    // just forwards events into a queue, and this loop (on our own thread,
    // where stdio is bound to `pipe`) does the actual printing.
    FuriMessageQueue* event_queue = furi_message_queue_alloc(16, sizeof(TouchpadTestEvent));
    FuriPubSub* pubsub = furi_record_open(RECORD_TOUCHPAD_TEST);
    FuriPubSubSubscription* subscription = furi_pubsub_subscribe(pubsub, hmi_test_touchpad_event_callback, event_queue);

    printf("Subscribed to touchpad test events. Press CTRL+C to stop.\r\n");

    // the app can also close itself (e.g. the Back button on the device), in
    // which case it publishes TouchpadTestStatusEnded from its own teardown;
    // stop waiting right away instead of sitting on a dead app until Ctrl+C
    bool app_running = true;
    bool test_ok = false;
    TouchpadTestEvent event;
    while(!test_ok && app_running && !cli_is_pipe_broken_or_is_etx_next_char(pipe)) {
        if(furi_message_queue_get(event_queue, &event, 100) == FuriStatusOk) {
            switch(event.tp_status) {
            case TouchpadTestStatusStarted:
                printf(ANSI_FG_YELLOW "test started" ANSI_RESET "\r\n");
                break;
            case TouchpadTestStatusEnded:
                printf(ANSI_FG_RED "test ended (app closed itself)" ANSI_RESET "\r\n");
                app_running = false;
                break;
            case TouchpadTestStatusCleared:
                printf(ANSI_FG_YELLOW "cleared" ANSI_RESET "\r\n");
                break;
            case TouchpadTestStatusDiamondFilled:
                printf("diamond filled at (%ld, %ld)\r\n", (long)event.diamond_x, (long)event.diamond_y);
                break;
            case TouchpadTestStatusAllFilled:
                printf(ANSI_FG_GREEN "all diamonds filled!" ANSI_RESET "\r\n");
                test_ok = true;
                break;
            default:
                printf("unknown event: %d\r\n", (int)event.tp_status);
                break;
            }
            stdio_flush();
        }
    }

    furi_pubsub_unsubscribe(pubsub, subscription);
    furi_record_close(RECORD_TOUCHPAD_TEST);
    furi_message_queue_free(event_queue);

    if(app_running) {
        // still running - we're the ones stopping it (Ctrl+C)
        printf("Stopping touch test...\r\n");
        desktop_stop_app();
    }

    if(test_ok) {
        printf(CLI_STATUS_OK);
    } else {
        printf(CLI_STATUS_ERROR);
    }

    return true;
}

static const HmiTestCmd hmi_test_cmds[] = {
    {"touch", "", "Start an application test touchpad", hmi_test_cli_start_touch},

};

static void hmi_test_command_cli_print_usage(void) {
    printf("Usage:\r\nhmi_test <cmd>\r\nCmd list:\r\n");
    for(size_t i = 0; i < COUNT_OF(hmi_test_cmds); i++) {
        const HmiTestCmd* c = &hmi_test_cmds[i];
        printf("\t%s %s - %s\r\n", c->name, c->arg_spec, c->description);
    }
}

void hmi_test_command_cli(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(context);
    FuriString* cmd = furi_string_alloc();
    bool handled = false;

    if(args_read_string_and_trim(args, cmd)) {
        const char* cmd_str = furi_string_get_cstr(cmd);
        for(size_t i = 0; i < COUNT_OF(hmi_test_cmds); i++) {
            const HmiTestCmd* c = &hmi_test_cmds[i];
            if(strcmp(cmd_str, c->name) == 0) {
                if(!c->execute(pipe, args)) {
                    printf("usage: hmi_test %s %s\r\n", c->name, c->arg_spec);
                }
                handled = true;
                break;
            }
        }
    }

    if(!handled) {
        hmi_test_command_cli_print_usage();
    }

    furi_string_free(cmd);
}
