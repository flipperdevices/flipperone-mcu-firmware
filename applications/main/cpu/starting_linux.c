#include "starting_linux.h"

#include <furi.h>
#include <gui/clay_helper.h>
#include <assets.h>

#define TAG "StartingLinux"

/* Positions from the mock: everything is centred on a 256 pixel wide screen,
 * the dolphin 20 pixels from the top and the bar 7 below it */
#define STARTING_LINUX_WIDTH      256
#define STARTING_LINUX_TOP        20
#define STARTING_LINUX_GAP        7
#define STARTING_LINUX_TEXT_Y     3 // the text box is taller than its letters
// The text stays where the mock has it with all three dots
#define STARTING_LINUX_TEXT_WIDTH 92

/* The thumb runs over the inside of the bar, from 1 to 97, left to right: it
 * comes in at the left edge and leaves at the right one, cut by both */
#define STARTING_LINUX_THUMB_WIDTH  35
#define STARTING_LINUX_THUMB_HEIGHT 8
#define STARTING_LINUX_INSIDE_START 1
#define STARTING_LINUX_INSIDE_END   97
#define STARTING_LINUX_RUN_MS       1000
#define STARTING_LINUX_DOT_MS       (STARTING_LINUX_RUN_MS / 4)

// Kept at zero width too: a missing row would move the ones below it up
static void starting_linux_rect(Clay_ElementId id, int32_t width, int32_t height) {
    CLAY(id, {.backgroundColor = COLOR_WHITE, .layout = {.sizing = {CLAY_SIZING_FIXED(MAX(width, 0)), CLAY_SIZING_FIXED(height)}}}) {
    }
}

// A white pill of the given width: its two top and bottom rows are shorter
static void starting_linux_thumb(int32_t width) {
    CLAY(
        CLAY_APP_ID("Thumb"),
        {.layout = {
             .layoutDirection = CLAY_TOP_TO_BOTTOM,
             .sizing = {CLAY_SIZING_FIXED(width), CLAY_SIZING_FIXED(STARTING_LINUX_THUMB_HEIGHT)},
             .childAlignment = {.x = CLAY_ALIGN_X_CENTER},
         }}) {
        starting_linux_rect(CLAY_APP_ID("ThumbRow0"), width - 4, 1);
        starting_linux_rect(CLAY_APP_ID("ThumbRow1"), width - 2, 1);
        starting_linux_rect(CLAY_APP_ID("ThumbMiddle"), width, STARTING_LINUX_THUMB_HEIGHT - 4);
        starting_linux_rect(CLAY_APP_ID("ThumbRow6"), width - 2, 1);
        starting_linux_rect(CLAY_APP_ID("ThumbRow7"), width - 4, 1);
    }
}

void starting_linux_layout(uint32_t elapsed_ms) {
    // Where the thumb's left edge is, from fully out on the left to fully out on the right
    const int32_t run = STARTING_LINUX_INSIDE_END - STARTING_LINUX_INSIDE_START + STARTING_LINUX_THUMB_WIDTH;
    const int32_t left =
        STARTING_LINUX_INSIDE_START - STARTING_LINUX_THUMB_WIDTH + (int32_t)((uint64_t)(elapsed_ms % STARTING_LINUX_RUN_MS) * run / STARTING_LINUX_RUN_MS);
    const int32_t start = MAX(left, STARTING_LINUX_INSIDE_START);
    const int32_t end = MIN(left + STARTING_LINUX_THUMB_WIDTH, STARTING_LINUX_INSIDE_END);

    CLAY(
        CLAY_APP_ID("Screen"),
        {
            .backgroundColor = COLOR_WHITE,
            .layout =
                {
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .sizing = {CLAY_SIZING_FIXED(STARTING_LINUX_WIDTH), CLAY_SIZING_GROW(0)},
                    .padding = {.top = STARTING_LINUX_TOP},
                    .childGap = STARTING_LINUX_GAP,
                    .childAlignment = {.x = CLAY_ALIGN_X_CENTER},
                },
        }) {
        clay_fixed_image(CLAY_APP_ID("Dolphin"), &starting_linux_dolphin);

        // The black bar, the thumb over it and the frame over both
        CLAY(
            CLAY_APP_ID("Bar"),
            {
                .layout =
                    {
                        .sizing = {CLAY_SIZING_FIXED(starting_linux_bar_bg.width), CLAY_SIZING_FIXED(starting_linux_bar_bg.height)},
                        .padding = {.left = start, .top = 1},
                    },
                .image = {.imageData = (void*)&starting_linux_bar_bg},
            }) {
            if(end > start) starting_linux_thumb(end - start);
            CLAY(
                CLAY_APP_ID("BarFrame"),
                {
                    .layout = {.sizing = {CLAY_SIZING_FIXED(starting_linux_bar_frame.width), CLAY_SIZING_FIXED(starting_linux_bar_frame.height)}},
                    .image = {.imageData = (void*)&starting_linux_bar_frame},
                    // Floating: drawn after the bar and the thumb, under later floating elements
                    .floating = {.attachTo = CLAY_ATTACH_TO_PARENT},
                }) {
            }
        }

        // No dots to three, one more every quarter of the thumb's run
        static const Clay_String texts[] = {
            CLAY_STRING_CONST("Starting Linux"),
            CLAY_STRING_CONST("Starting Linux."),
            CLAY_STRING_CONST("Starting Linux.."),
            CLAY_STRING_CONST("Starting Linux..."),
        };
        CLAY(
            CLAY_APP_ID("Text"),
            {.layout = {
                 .sizing = {.width = CLAY_SIZING_FIXED(STARTING_LINUX_TEXT_WIDTH)},
                 .padding = {.top = STARTING_LINUX_TEXT_Y},
             }}) {
            CLAY_TEXT(
                texts[(elapsed_ms % STARTING_LINUX_RUN_MS) / STARTING_LINUX_DOT_MS],
                CLAY_TEXT_CONFIG({.fontId = FontBig, .textColor = COLOR_BLACK, .wrapMode = CLAY_TEXT_WRAP_NONE}));
        }
    }
}
