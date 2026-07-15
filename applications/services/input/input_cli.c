#include <furi/furi.h>

#include <input/input.h>
#include <input_touch/input_touch.h>
#include <containers/pipe.h>
#include <cli/args.h>
#include <cli/cli_command.h>
#include <toolbox/strint.h>

static void input_cli_print_usage(PipeSide* pipe, void* context) {
    UNUSED(pipe);
    UNUSED(context);

    printf("Usage: input <command>\r\n");
    printf("Commands:\r\n");
    printf("\tdump - Dumps incoming input events\r\n");
    printf("\tsend <key> <type> - Inject input\r\n");
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

    printf("\ttouch <type> <x> <y> <pressure> - Inject touch input\r\n");
    printf("\t\t<type> = ");
    for(size_t i = 0; i < InputTouchTypeMAX; i++) {
        printf("%s%s", input_touch_get_type_name(i), i == InputTouchTypeMAX - 1 ? "" : ", ");
    }
    printf("\r\n");
    printf("\t\t<x> = 0..%d, <y> = 0..%d, <pressure> = 0..%d\r\n", TOUCHPAD_RESOLUTION_X, TOUCHPAD_RESOLUTION_Y, TOUCHPAD_RESOLUTION_PRESSURE);
}

typedef enum {
    InputCliDumpEventTypeKey,
    InputCliDumpEventTypeTouch,
} InputCliDumpEventType;

typedef struct {
    InputCliDumpEventType type;
    union {
        InputEvent key;
        InputTouchEvent touch;
    } event;
} InputCliDumpEvent;

static void input_cli_dump_events_callback(const void* value, void* ctx) {
    furi_assert(value);
    furi_assert(ctx);
    FuriMessageQueue* input_queue = ctx;
    InputCliDumpEvent ev = {.type = InputCliDumpEventTypeKey, .event.key = *(const InputEvent*)value};
    furi_message_queue_put(input_queue, &ev, FuriWaitForever);
}

static void input_cli_dump_touch_events_callback(const void* value, void* ctx) {
    furi_assert(value);
    furi_assert(ctx);
    FuriMessageQueue* input_queue = ctx;
    InputCliDumpEvent ev = {.type = InputCliDumpEventTypeTouch, .event.touch = *(const InputTouchEvent*)value};
    furi_message_queue_put(input_queue, &ev, FuriWaitForever);
}

static void input_cli_dump(PipeSide* pipe) {
    FuriMessageQueue* input_queue = furi_message_queue_alloc(8, sizeof(InputCliDumpEvent));

    FuriPubSub* input_events = furi_record_open(RECORD_INPUT_EVENTS);
    FuriPubSubSubscription* input_subscription = furi_pubsub_subscribe(input_events, input_cli_dump_events_callback, input_queue);

    FuriPubSub* touch_events = furi_record_open(RECORD_INPUT_TOUCH_EVENTS);
    FuriPubSubSubscription* touch_subscription = furi_pubsub_subscribe(touch_events, input_cli_dump_touch_events_callback, input_queue);

    InputCliDumpEvent ev;
    printf("Press CTRL+C to stop\r\n");
    while(!cli_is_pipe_broken_or_is_etx_next_char(pipe)) {
        if(furi_message_queue_get(input_queue, &ev, 100) == FuriStatusOk) {
            if(ev.type == InputCliDumpEventTypeKey) {
                printf("key: %s type: %s\r\n", input_get_key_name(ev.event.key.key), input_get_type_name(ev.event.key.type));
            } else {
                printf(
                    "touch: %s x=%ld y=%ld p=%ld\r\n",
                    input_touch_get_type_name(ev.event.touch.type),
                    ev.event.touch.x,
                    ev.event.touch.y,
                    ev.event.touch.pressure);
            }
            fflush(stdout);
        }
    }

    furi_pubsub_unsubscribe(touch_events, touch_subscription);
    furi_record_close(RECORD_INPUT_TOUCH_EVENTS);

    furi_pubsub_unsubscribe(input_events, input_subscription);
    furi_record_close(RECORD_INPUT_EVENTS);

    furi_message_queue_free(input_queue);
}

static void input_cli_command_send(PipeSide* pipe, FuriString* args) {
    UNUSED(pipe);
    UNUSED(args);

    FuriPubSub* input_events = furi_record_open(RECORD_INPUT_EVENTS);
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
    furi_record_close(RECORD_INPUT_EVENTS);
}

static void input_cli_command_touch(PipeSide* pipe, FuriString* args) {
    UNUSED(pipe);
    UNUSED(args);

    FuriPubSub* touch_events = furi_record_open(RECORD_INPUT_TOUCH_EVENTS);

    //InputKey key = InputKeyUp;
    InputTouchType type = InputTouchTypeEnd;
    FuriString* str_tmp = furi_string_alloc();
    int32_t x = 0xFFFF;
    int32_t y = 0xFFFF;
    int32_t pressure = 0;

    bool args_parsed = false;
    do {
        size_t i = 0;

        if(!args_read_string_and_trim(args, str_tmp)) break;
        for(i = 0; i < InputTouchTypeMAX; i++) {
            if(furi_string_cmp_str(str_tmp, input_touch_get_type_name(i)) == 0) break;
        }
        if(i == InputTouchTypeMAX) break;
        type = i;

        if(i != InputTouchTypeEnd) {
            const char* args_cstr = furi_string_get_cstr(args);
            StrintParseError parse_err = StrintParseNoError;
            parse_err |= strint_to_int32(args_cstr, &args_cstr, &x, 10);
            parse_err |= strint_to_int32(args_cstr, &args_cstr, &y, 10);
            parse_err |= strint_to_int32(args_cstr, &args_cstr, &pressure, 10);
            if(parse_err != StrintParseNoError || x < 0 || x > TOUCHPAD_RESOLUTION_X || y < 0 || y > TOUCHPAD_RESOLUTION_Y || pressure < 0 ||
               pressure > TOUCHPAD_RESOLUTION_PRESSURE) {
                printf("Invalid coordinates: %s\r\n", furi_string_get_cstr(args));
                break;
            }
        }

        args_parsed = true;
    } while(false);

    if(!args_parsed) {
        input_cli_print_usage(pipe, NULL);
    } else {
        InputTouchEvent event = {
            .type = type,
            .x = x,
            .y = y,
            .pressure = pressure,
        };
        furi_pubsub_publish(touch_events, &event);
    }

    furi_string_free(str_tmp);
    furi_record_close(RECORD_INPUT_TOUCH_EVENTS);
}

void input_cli_command(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(context);

    FuriString* cmd = furi_string_alloc();

    bool cmd_parsed = false;
    do {
        if(!args_read_string_and_trim(args, cmd)) break;
        if(furi_string_cmp_str(cmd, "dump") == 0) {
            input_cli_dump(pipe);
        } else if(furi_string_cmp_str(cmd, "send") == 0) {
            input_cli_command_send(pipe, args);
        } else if(furi_string_cmp_str(cmd, "touch") == 0) {
            input_cli_command_touch(pipe, args);
        } else {
            break;
        }

        cmd_parsed = true;
    } while(false);

    if(!cmd_parsed) {
        input_cli_print_usage(pipe, NULL);
    }

    furi_string_free(cmd);
}
