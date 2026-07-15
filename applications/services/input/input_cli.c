#include <furi/furi.h>

#include <input/input.h>
#include <containers/pipe.h>
#include <cli/args.h>
#include <cli/cli_command.h>

static void input_cli_print_usage(PipeSide* pipe, void* context) {
    UNUSED(pipe);
    UNUSED(context);

    printf("Usage: input <command>\r\n");
    printf("Commands:\r\n");
    printf("\tdump\t\t\t- Dumps incoming input events\r\n");
    printf("\tsend\t<key> <type>\t- Inject input\r\n");
    printf("\t\t<key> = ");
    for(size_t i = 0; i < input_pins_count; i++) {
        printf("%s%s", input_pins[i].name, i == input_pins_count - 1 ? "" : ", ");
    }
    printf("\r\n");
    printf("\t\t<type> = ");
    for(size_t i = 0; i < InputTypeMAX; i++) {
        printf("%s%s", input_get_type_name(i), i == InputTypeMAX - 1 ? "" : ", ");
    }
    printf("\r\n");
}

static void input_cli_dump_events_callback(const void* value, void* ctx) {
    furi_assert(value);
    furi_assert(ctx);
    FuriMessageQueue* input_queue = ctx;
    furi_message_queue_put(input_queue, value, FuriWaitForever);
}

static void input_cli_dump(PipeSide* pipe, FuriPubSub* input_events) {
    FuriMessageQueue* input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    FuriPubSubSubscription* input_subscription =
        furi_pubsub_subscribe(input_events, input_cli_dump_events_callback, input_queue);

    InputEvent input_event;
    printf("Press CTRL+C to stop\r\n");
    while(!cli_is_pipe_broken_or_is_etx_next_char(pipe)) {
        if(furi_message_queue_get(input_queue, &input_event, 100) == FuriStatusOk) {
            printf(
                "key: %s type: %s\r\n",
                input_get_key_name(input_event.key),
                input_get_type_name(input_event.type));
            fflush(stdout);
        }
    }

    furi_pubsub_unsubscribe(input_events, input_subscription);
    furi_message_queue_free(input_queue);
}

static void input_cli_command_send(PipeSide* pipe, FuriString* args, FuriPubSub* input_events) {
    UNUSED(pipe);
    UNUSED(args);

    InputKey key = InputKeyUp;
    InputType type = InputTypePress;
    FuriString* str_tmp = furi_string_alloc();

    bool args_parsed = false;
    do {
        if(!args_read_string_and_trim(args, str_tmp)) break;
        size_t i = 0;
        for(i = 0; i < input_pins_count; i++) {
            if(furi_string_cmp_str(str_tmp, input_pins[i].name) == 0) break;
        }
        if(i == input_pins_count) break;
        key = input_pins[i].key;

        if(!args_read_string_and_trim(args, str_tmp)) break;
        for(i = 0; i < InputTypeMAX; i++) {
            if(furi_string_cmp_str(str_tmp, input_get_type_name(i)) == 0) break;
        }
        if(i == InputTypeMAX) break;
        type = i;

        args_parsed = true;
    } while(false);

    if(!args_parsed) {
        input_cli_print_usage(pipe, NULL);
    } else {
        InputEvent event = {
            .key = key,
            .type = type,
        };
        furi_pubsub_publish(input_events, &event);
    }

    furi_string_free(str_tmp);
}

void input_cli_command(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(context);

    FuriString* cmd = furi_string_alloc();
    FuriPubSub* input_events = furi_record_open(RECORD_INPUT_EVENTS);

    bool cmd_parsed = false;
    do {
        if(!args_read_string_and_trim(args, cmd)) break;
        if(furi_string_cmp_str(cmd, "dump") == 0) {
            input_cli_dump(pipe, input_events);
        } else if(furi_string_cmp_str(cmd, "send") == 0) {
            input_cli_command_send(pipe, args, input_events);
        } else {
            break;
        }

        cmd_parsed = true;
    } while(false);

    if(!cmd_parsed) {
        input_cli_print_usage(pipe, NULL);
    }

    furi_string_free(cmd);
    furi_record_close(RECORD_INPUT_EVENTS);
}
