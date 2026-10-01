#include "hmi_test_cli.h"

#include <pico/stdio.h>

#include <applications.h>
#include <desktop/desktop.h>
#include <debug/touchpad_test_v2/touchpad_test.h>
#include <haptic/haptic.h>

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

typedef struct {
    FuriMessageQueue* queue;
    uint32_t dropped;
} HmiTestTouchpadEventCtx;

static void hmi_test_touchpad_event_callback(const void* value, void* ctx) {
    furi_assert(value);
    furi_assert(ctx);
    HmiTestTouchpadEventCtx* event_ctx = ctx;
    TouchpadTestEvent event = *(const TouchpadTestEvent*)value;
    // furi_pubsub_publish() holds the pubsub's mutex for the duration of this
    // call, and for events published from touchpad_test_v2's touch handlers
    // that's on the GUI thread, under gui->mutex and view->mutex too - so a
    // FuriWaitForever put here, if the queue were ever full, could deadlock
    // against whichever thread would otherwise drain it (e.g. us, if we're
    // blocked in furi_pubsub_unsubscribe() waiting on that same mutex). Bound
    // the wait and just count drops instead; in practice the queue (16 deep)
    // shouldn't fill since duplicate diamond hits are already deduplicated
    // before this is invoked.
    if(furi_message_queue_put(event_ctx->queue, &event, 20) != FuriStatusOk) {
        event_ctx->dropped++;
    }
}

static bool hmi_test_cli_start_touch(PipeSide* pipe, FuriString* args) {
    furi_string_trim(args);
    if(furi_string_size(args) > 0) {
        return false;
    }

    const char* appid = "touchpad_test_v2";

    const FlipperInternalApplication* target = hmi_test_find_app_by_appid(appid);
    if(!target) {
        printf(ANSI_FG_RED "Application not found: %s" ANSI_RESET "\r\n", appid);
        printf(CLI_STATUS_ERROR);
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
    HmiTestTouchpadEventCtx event_ctx = {
        .queue = furi_message_queue_alloc(16, sizeof(TouchpadTestEvent)),
        .dropped = 0,
    };
    FuriPubSub* pubsub = furi_record_open(RECORD_TOUCHPAD_TEST);
    FuriPubSubSubscription* subscription = furi_pubsub_subscribe(pubsub, hmi_test_touchpad_event_callback, &event_ctx);

    printf("Subscribed to touchpad test events. Press CTRL+C to stop.\r\n");

    // the app can also close itself (e.g. the Back button on the device), in
    // which case it publishes TouchpadTestStatusEnded from its own teardown;
    // stop waiting right away instead of sitting on a dead app until Ctrl+C
    bool app_running = true;
    bool test_ok = false;
    TouchpadTestEvent event;
    while(!test_ok && app_running && !cli_is_pipe_broken_or_is_etx_next_char(pipe)) {
        if(furi_message_queue_get(event_ctx.queue, &event, 100) == FuriStatusOk) {
            switch(event.tp_status) {
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
    furi_message_queue_free(event_ctx.queue);

    if(event_ctx.dropped > 0) {
        printf(ANSI_FG_YELLOW "dropped %lu event(s)" ANSI_RESET "\r\n", (unsigned long)event_ctx.dropped);
    }

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

static bool hmi_test_cli_haptic(PipeSide* pipe, FuriString* args) {
    UNUSED(pipe);

    int effect_id = 0;
    if(!args_read_int_and_trim(args, &effect_id) || effect_id < 0 || (effect_id >= Drv2605lEffectCountMax && effect_id != 255)) {
        printf(CLI_STATUS_ERROR);
        return false;
    }

    int duration = 0;
    if(!furi_string_empty(args)) {
        if(!args_read_int_and_trim(args, &duration) || duration < 0) {
            printf(CLI_STATUS_ERROR);
            return false;
        }
        if(duration == 1) {
            duration = 2;
        }
    }

    bool ret = false;
    Haptic* haptic = furi_record_open(RECORD_HAPTIC);
    if(effect_id == 255) {
        printf("Calibrating haptic device...\r\n");
        ret = haptic_force_auto_calibrate(haptic);
    } else {
        printf("Testing haptic effect %d for duration %d ms\r\n", effect_id, duration);
        ret = haptic_play_effect(haptic, (Drv2605lEffect)effect_id, duration);
        printf("Haptic effect played.\r\n");
    }

    furi_record_close(RECORD_HAPTIC);

    if(!ret) {
        // Arguments were fine, so keep the caller from printing usage here.
        // Calibration now returns the real outcome: false means either the
        // device isn't initialised or the calibration itself failed.
        printf(
            ANSI_FG_RED "Haptic %s failed" ANSI_RESET "\r\n",
            effect_id == 255 ? "calibration" : "playback");
        printf(CLI_STATUS_ERROR);
        return true;
    }

    printf(CLI_STATUS_OK);
    return true;
}

static const HmiTestCmd hmi_test_cmds[] = {
    {"touch", "", "Start an application test touchpad", hmi_test_cli_start_touch},
    {"haptic",
     "<effect_id> [duration]",
     "test haptic, effect_id = 0..123 (255 - calibration), duration = 0 (service default, 3s) or play time in ms (min 2)",
     hmi_test_cli_haptic}};

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
