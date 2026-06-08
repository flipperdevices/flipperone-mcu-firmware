#include "cli_command_screen.h"

#include "cli_ansi.h"
#include "furi_hal_resources.h"
#include "gui/gui_i.h"
#include "gui/gui.h"

typedef struct {
    uint8_t* local_framebuffer;
    size_t framebuffer_size;
    FuriMutex* mutex;
    bool has_new_frame;
} CliScreenContext;

static void cli_screen_framebuffer_callback(const uint8_t* data, size_t width, size_t height, void* context) {
    CliScreenContext* ctx = context;
    if (!ctx || !data) return;

    const size_t incomming_size = width * height;
    furi_check(incomming_size == ctx->framebuffer_size); //size must be equal to full buffer size
    
    if (furi_mutex_acquire(ctx->mutex, 16) == FuriStatusOk) {
        memcpy(ctx->local_framebuffer, data, width * height);
        ctx->has_new_frame = true;
        
        furi_mutex_release(ctx->mutex);
    }
}

static void cli_screen_emulate_click(FuriPubSub* input_pubsub, InputKey key) {
    InputEvent press_event = {
        .sequence_source = INPUT_SEQUENCE_SOURCE_SOFTWARE,
        .sequence_counter = 0,
        .key = key,
        .type = InputTypePress
    };
    furi_pubsub_publish(input_pubsub, &press_event);
    
    furi_delay_ms(40);

    InputEvent release_event = {
        .sequence_source = INPUT_SEQUENCE_SOURCE_SOFTWARE,
        .sequence_counter = 0,
        .key = key,
        .type = InputTypeRelease
    };
    furi_pubsub_publish(input_pubsub, &release_event);
}

static bool cli_command_screen_process_input(PipeSide* pipe, CliAnsiParser* ansi_parser, FuriPubSub* input_pubsub) {
    while(pipe_bytes_available(pipe) > 0) {
        char ch = getchar();
        if (ch == 0x03) return true;

        CliAnsiParserResult ansi_res = cli_ansi_parser_feed(ansi_parser, ch);
        if (ansi_res.is_done) {
            InputKey emulated_key = InputKeyMask;

            switch ((int) ansi_res.result.key) {
                case CliKeyUp:
                case 'W':
                case 'w':
                    emulated_key = InputKeyUp; break;
                case CliKeyDown:
                case 'S':
                case 's':
                    emulated_key = InputKeyDown; break;
                case CliKeyLeft:
                case 'A':
                case 'a':
                    emulated_key = InputKeyLeft; break;
                case CliKeyRight:
                case 'D':
                case 'd':
                    emulated_key = InputKeyRight; break;
                case CliKeyEsc:
                case 0x07F:
                case 0x08:
                case 'B':
                case 'b':
                    emulated_key = InputKeyBack; break;
                case '\r':
                case '\n':
                case ' ':
                case 'O':
                case 'o':
                    emulated_key = InputKeyOk; break;
                case '\t':
                case 'X':
                case 'x':
                    emulated_key = InputKeySw; break;
                case 'P':
                case 'p':
                    emulated_key = InputKeyPtt; break; 
                case '1': 
                    emulated_key = InputKey1; break;
                case '2': 
                    emulated_key = InputKey2; break;
                case '3': 
                    emulated_key = InputKey3; break;
                case '4': 
                    emulated_key = InputKey4; break;
                case '5': 
                    emulated_key = InputKey5; break;
                default:
                    break;
            }

            if(emulated_key != InputKeyMask) {
                cli_screen_emulate_click(input_pubsub, emulated_key);
            }
        }
    }
    return false;
}

static void cli_command_screen_render_braille(PipeSide* pipe, const uint8_t* fb, 
    char* line_buffer, size_t line_buffer_size, size_t width, size_t height) {
    printf("\033[H");

    for(size_t y = 0; y < height; y += 4) {
        size_t line_pos = 0;
        line_buffer[line_pos++] = '|';

        const uint8_t* r0 = &fb[(y + 0) * width];
        const uint8_t* r1 = &fb[(y + 1) * width];
        const uint8_t* r2 = &fb[(y + 2) * width];
        const uint8_t* r3 = &fb[(y + 3) * width];

        for (size_t x = 0; x < width; x+=2) {
            furi_check((line_pos + 3) <= line_buffer_size);
            
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

            line_buffer[line_pos++] = 0xE2;
            line_buffer[line_pos++] = 0xA0 + (braille_byte >> 6);
            line_buffer[line_pos++] = 0x80 + (braille_byte & 0x3F);
        }

        furi_check((line_pos + 4) <= line_buffer_size);
        line_buffer[line_pos++] = '|';
        line_buffer[line_pos++] = '\r';
        line_buffer[line_pos++] = '\n';
        line_buffer[line_pos]   = '\0';

        pipe_send(pipe, (uint8_t*)line_buffer, line_pos);
    }
}

void cli_command_screen(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(args);
    UNUSED(context);

    Gui* gui = furi_record_open(RECORD_GUI);
    FuriPubSub* input_pubsub = furi_record_open(RECORD_INPUT_EVENTS);

    const size_t width = gui_get_width(gui);
    const size_t height = gui_get_height(gui);

    const size_t framebuffer_size = height * width;
    uint8_t* framebuffer = malloc(framebuffer_size);
    const size_t line_buffer_size = (width / 2) * 3 + 10;
    char* line_buffer = malloc(line_buffer_size);

    CliScreenContext screen_ctx = {
        .local_framebuffer = framebuffer,
        .framebuffer_size = framebuffer_size,
        .mutex = furi_mutex_alloc(FuriMutexTypeNormal),
        .has_new_frame = false
    };

    CliAnsiParser* ansi_parser = cli_ansi_parser_alloc();

    printf("Controls: Arrows = Navigate, Enter = Ok, Backspace = Back\r\n");
    printf("Press CTRL+C to stop... \r\n");
    furi_delay_ms(500);

    gui_add_framebuffer_callback(gui, cli_screen_framebuffer_callback, &screen_ctx);
    gui_update(gui);

    printf("\033[2J\033[H\033[?25l");
    while(pipe_bytes_available(pipe) > 0) getchar(); 

    while(pipe_state(pipe) != PipeStateBroken) {
        if (cli_command_screen_process_input(pipe, ansi_parser, input_pubsub)) {
            break;
        }

        if (furi_mutex_acquire(screen_ctx.mutex, FuriWaitForever) == FuriStatusOk) {
            if (screen_ctx.has_new_frame) {
                screen_ctx.has_new_frame = false;    
                cli_command_screen_render_braille(pipe, screen_ctx.local_framebuffer, line_buffer, line_buffer_size, width, height);
            }
            furi_mutex_release(screen_ctx.mutex);
        }
        
        furi_delay_ms(40);
    }

    printf("\033[2J\033[H\033[?25h");
    
    gui_remove_framebuffer_callback(gui, cli_screen_framebuffer_callback, &screen_ctx);
    cli_ansi_parser_free(ansi_parser);
    furi_mutex_free(screen_ctx.mutex);

    free(framebuffer);
    free(line_buffer);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_INPUT_EVENTS);
}