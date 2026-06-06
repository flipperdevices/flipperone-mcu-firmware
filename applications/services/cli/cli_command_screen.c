#include "cli_command_screen.h"

#include "cli_command.h"
#include "gui/gui_i.h"
#include "gui/gui.h"

typedef struct {
    uint8_t* local_framebuffer;
    FuriMutex* mutex;
    bool has_new_frame;
} CliScreenContext;

static void cli_screen_framebuffer_callback(const uint8_t* data, size_t width, size_t height, void* context) {
    CliScreenContext* ctx = context;
    if (!ctx || !data) return;

    if (furi_mutex_acquire(ctx->mutex, 0) == FuriStatusOk) {
        memcpy(ctx->local_framebuffer, data, width * height);
        ctx->has_new_frame = true;
        
        furi_mutex_release(ctx->mutex);
    }
}

void cli_command_screen(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(args);
    UNUSED(context);

    Gui* gui = furi_record_open(RECORD_GUI);
    if (!gui) {
        printf("Error: GUI service not found\r\n");
        return;
    }

    const ssize_t width = gui_get_width(gui);
    const ssize_t height = gui_get_height(gui);

    uint8_t* framebuffer = malloc(height * width);
    size_t line_buffer_size = (width / 2) * 3 + 10;
    char* line_buffer = malloc(line_buffer_size);
    
    if (!framebuffer || !line_buffer) {
        printf("Error: Memory allocation");
        if (framebuffer) free(framebuffer);
        if (line_buffer) free(line_buffer);
        furi_record_close(RECORD_GUI);
        return;
    }

    CliScreenContext screen_ctx = {
        .local_framebuffer = framebuffer,
        .mutex = furi_mutex_alloc(FuriMutexTypeNormal),
        .has_new_frame = false
    };

    printf("Press CTRL+C to stop... \r\n");
    furi_delay_ms(500);

    gui_add_framebuffer_callback(gui, cli_screen_framebuffer_callback, &screen_ctx);
    gui_update(gui);
    // screen_ctx.has_new_frame = true;

    printf("\033[2J\033[H\033[?25l");

    while(!cli_is_pipe_broken_or_is_etx_next_char(pipe)) {

        if (furi_mutex_acquire(screen_ctx.mutex, FuriWaitForever) == FuriStatusOk) {
            if (screen_ctx.has_new_frame) {
                screen_ctx.has_new_frame = false;

                printf("\033[H");

                for (size_t y = 0; y < height; y+=4) {
                    size_t line_pos = 0;
                    line_buffer[line_pos++] = '|';
    
                    const uint8_t* r0 = &screen_ctx.local_framebuffer[(y + 0) * width];
                    const uint8_t* r1 = &screen_ctx.local_framebuffer[(y + 1) * width];
                    const uint8_t* r2 = &screen_ctx.local_framebuffer[(y + 2) * width];
                    const uint8_t* r3 = &screen_ctx.local_framebuffer[(y + 3) * width];
    
                    for (size_t x = 0; x < width; x+=2) {
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
    
                    line_buffer[line_pos++] = '|';
                    line_buffer[line_pos++] = '\r';
                    line_buffer[line_pos++] = '\n';
                    line_buffer[line_pos]   = '\0';
    
                    pipe_send(pipe, (uint8_t*)line_buffer, line_pos);
                }
            }
            furi_mutex_release(screen_ctx.mutex);
        }
        
        furi_delay_ms(50);
    }

    printf("\033[2J\033[H\033[?25h");
    gui_remove_framebuffer_callback(gui, cli_screen_framebuffer_callback, &screen_ctx);
    furi_mutex_free(screen_ctx.mutex);

    free(framebuffer);
    free(line_buffer);

    furi_record_close(RECORD_GUI);
}