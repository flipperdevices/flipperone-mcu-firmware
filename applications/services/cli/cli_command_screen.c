#include "cli_command_screen.h"

#include "cli_ansi.h"
#include "furi_hal_resources.h"
#include "gui/gui_i.h"
#include "gui/gui.h"

#define SCREEN_CUSTOM_EVENT_NEW_FRAME (1U << 0)

typedef struct {
    Gui* gui;
    FuriPubSub* input_pubsub;
    FuriEventLoop* event_loop;
    PipeSide* pipe;
    CliAnsiParser* ansi_parser;

    uint8_t* framebuffer;
    size_t framebuffer_size;
    size_t width;
    size_t height;

    char* linebuffer;
    size_t linebuffer_size;

    FuriMutex* mutex;
} CommandScreen;

static void cli_command_screen_gui_update_callback(const uint8_t* data, size_t width, size_t height, void* context) {
    CommandScreen* instance = context;
    furi_check(instance);
    furi_check(data);

    furi_check(width * height == instance->framebuffer_size); //size must be equal to full buffer size

    if (furi_mutex_acquire(instance->mutex, 16) == FuriStatusOk) {
        memcpy(instance->framebuffer, data, instance->framebuffer_size);
        furi_mutex_release(instance->mutex);

        furi_event_loop_set_custom_event(instance->event_loop, SCREEN_CUSTOM_EVENT_NEW_FRAME);
    }
}

static void cli_command_screen_render_callback(uint32_t events, void* context) {
    CommandScreen* instance = context;
    furi_check(instance);

    if (events & SCREEN_CUSTOM_EVENT_NEW_FRAME) {
        if (furi_mutex_acquire(instance->mutex, FuriWaitForever) == FuriStatusOk) {
            printf(ANSI_CURSOR_POS("1", "1"));

            for (size_t y = 0; y < instance->height; y+=4) {
                size_t line_pos = 0;
                instance->linebuffer[line_pos++] = '|';

                const uint8_t* r0 = &instance->framebuffer[(y + 0) * instance->width];
                const uint8_t* r1 = &instance->framebuffer[(y + 1) * instance->width];
                const uint8_t* r2 = &instance->framebuffer[(y + 2) * instance->width];
                const uint8_t* r3 = &instance->framebuffer[(y + 3) * instance->width];

                for (size_t x = 0; x < instance->width; x += 2) {
                    furi_check((line_pos + 3) <= instance->linebuffer_size);

                    bool p1 = r0[x + 0] > 127;
                    bool p2 = r1[x + 0] > 127;
                    bool p3 = r2[x + 0] > 127;
                    bool p4 = r0[x + 1] > 127;
                    bool p5 = r1[x + 1] > 127;
                    bool p6 = r2[x + 1] > 127;
                    bool p7 = r3[x + 0] > 127;
                    bool p8 = r3[x + 1] > 127;
                    
                    uint8_t braille_byte = (p1 << 0) | (p2 << 1) | (p3 << 2) |
                                           (p4 << 3) | (p5 << 4) | (p6 << 5) |
                                           (p7 << 6) | (p8 << 7);
        
                    instance->linebuffer[line_pos++] = 0xE2;
                    instance->linebuffer[line_pos++] = 0xA0 + (braille_byte >> 6);
                    instance->linebuffer[line_pos++] = 0x80 + (braille_byte & 0x3F);
                }

                furi_check((line_pos + 4) <= instance->linebuffer_size);
                instance->linebuffer[line_pos++] = '|';
                instance->linebuffer[line_pos++] = '\r';
                instance->linebuffer[line_pos++] = '\n';
                instance->linebuffer[line_pos]   = '\0';

                pipe_send(instance->pipe, (uint8_t*)instance->linebuffer, line_pos);
            }
            
            furi_mutex_release(instance->mutex);
        }
    }
}

static void cli_command_screen_emulate_click(FuriPubSub* input_pubsub, InputKey key) {
    InputEvent event = {
        .sequence_source = INPUT_SEQUENCE_SOURCE_SOFTWARE,
        .sequence_counter = 0,
        .key = key,
        .type = InputTypePress
    };
    furi_pubsub_publish(input_pubsub, &event);
    
    furi_delay_ms(40);
    event.type = InputTypeRelease;
    furi_pubsub_publish(input_pubsub, &event);
}

static void cli_command_screen_click_callback(PipeSide* pipe, void* context) {
    CommandScreen* instance = context;
    furi_check(instance);

    while (pipe_bytes_available(pipe) > 0) {
        char ch = getchar();
        if (ch == 0x03) {
            furi_event_loop_stop(instance->event_loop); 
            return;
        }

        CliAnsiParserResult ansi_res = cli_ansi_parser_feed(instance->ansi_parser, ch);
        if (ansi_res.is_done) {
            InputKey emulated_key = InputKeyMask;

            switch ((int) ansi_res.result.key) {
                case CliKeyUp:    case 'W': case 'w': emulated_key = InputKeyUp; break;
                case CliKeyDown:  case 'S': case 's': emulated_key = InputKeyDown; break;
                case CliKeyLeft:  case 'A': case 'a': emulated_key = InputKeyLeft; break;
                case CliKeyRight: case 'D': case 'd': emulated_key = InputKeyRight; break;

                case CliKeyEsc: case 0x7F: case 0x08: case 'B': case 'b': emulated_key = InputKeyBack; break;
                case '\r': case '\n': case ' ': case 'O': case 'o': emulated_key = InputKeyOk; break;

                case '\t': case 'X': case 'x': emulated_key = InputKeySw; break;
                case 'P': case 'p': emulated_key = InputKeyPtt; break;

                case '1': emulated_key = InputKey1; break;
                case '2': emulated_key = InputKey2; break;
                case '3': emulated_key = InputKey3; break;
                case '4': emulated_key = InputKey4; break;
                case '5': emulated_key = InputKey5; break;

                default: break;
            }

            if (emulated_key != InputKeyMask) {
                cli_command_screen_emulate_click(instance->input_pubsub, emulated_key);
            }
        }
    }
}

static void cli_command_screen_pipe_broken_callback(PipeSide* pipe, void* context) {
    CommandScreen* instance = context;
    furi_check(instance);
    UNUSED(pipe);
    furi_event_loop_stop(instance->event_loop);
}

static CommandScreen* cli_command_screen_alloc(PipeSide* pipe) {
    CommandScreen* instance = malloc(sizeof(CommandScreen));
    instance->gui = furi_record_open(RECORD_GUI);
    instance->input_pubsub = furi_record_open(RECORD_INPUT_EVENTS);
    instance->event_loop = furi_event_loop_alloc();
    instance->pipe = pipe;
    instance->ansi_parser = cli_ansi_parser_alloc();

    instance->width = gui_get_width(instance->gui);
    instance->height = gui_get_height(instance->gui);
    instance->framebuffer_size = instance->width * instance->height;
    instance->linebuffer_size = (instance->width / 2) * 3 + 10;

    instance->framebuffer = malloc(instance->framebuffer_size);
    instance->linebuffer = malloc(instance->linebuffer_size);

    instance->mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    pipe_attach_to_event_loop(instance->pipe, instance->event_loop);
    pipe_set_callback_context(instance->pipe, instance);
    pipe_set_data_arrived_callback(instance->pipe, cli_command_screen_click_callback, FuriEventLoopEventFlagEdge);
    pipe_set_broken_callback(instance->pipe, cli_command_screen_pipe_broken_callback, 0);

    furi_event_loop_set_custom_event_callback(instance->event_loop, cli_command_screen_render_callback, instance);
 
    gui_add_framebuffer_callback(instance->gui, cli_command_screen_gui_update_callback, instance);
    
    return instance;
}

static void cli_command_screen_free(CommandScreen* instance) {
    gui_remove_framebuffer_callback(instance->gui, cli_command_screen_gui_update_callback, instance);

    pipe_detach_from_event_loop(instance->pipe);

    cli_ansi_parser_free(instance->ansi_parser);
    furi_mutex_free(instance->mutex);
    free(instance->framebuffer);
    free(instance->linebuffer);
    
    furi_event_loop_free(instance->event_loop);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_INPUT_EVENTS);
    free(instance);
}

void cli_command_screen(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(args);
    UNUSED(context);

    printf("Controls: Arrow = Navigate, Enter = Ok, Backspace = Back\r\n");
    printf("Press CTRL+C to stop...\r\n");
    furi_delay_ms(500);
    
    while(pipe_bytes_available(pipe) > 0) getchar();
    
    CommandScreen* instance = cli_command_screen_alloc(pipe);

    printf(ANSI_ERASE_DISPLAY(ANSI_ERASE_ENTIRE) ANSI_CURSOR_POS("1", "1") ANSI_CURSOR_HIDE);
    gui_update(instance->gui);
    
    furi_event_loop_run(instance->event_loop);

    printf(ANSI_ERASE_DISPLAY(ANSI_ERASE_ENTIRE) ANSI_CURSOR_POS("1", "1") ANSI_CURSOR_SHOW);
    cli_command_screen_free(instance);
}