#include <assets.h>
#include <furi.h>
#include <gui/clay_helper.h>
#include "elements.h"

void elements_softkey_button_element(const char* text, SoftkeyButtonState state) {
    const Image* image;
    switch(state) {
    case SoftkeyButtonStateInactive:
        image = &button_inactive;
        break;
    case SoftkeyButtonStateActive:
        image = &button_active;
        break;
    case SoftkeyButtonStatePressed:
        image = &button_pressed;
        break;
    default:
        furi_crash();
    }

    const Clay_Sizing sizing = {.width = CLAY_SIZING_FIXED(image->width), .height = CLAY_SIZING_FIXED(image->height)};

    CLAY_AUTO_ID({
        .layout =
            {
                .sizing = sizing,
            },
    }) {
        CLAY_AUTO_ID({
            .layout =
                {
                    .sizing = sizing,
                    .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                    .padding = {.left = 0, .right = 0, .top = 3, .bottom = 0},
                },
            .image = {.imageData = (void*)image},
        }) {
            CLAY_TEXT(
                clay_helper_string_from_chars(text),
                CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = state == SoftkeyButtonStatePressed ? COLOR_WHITE : COLOR_BLACK}));
        }
    }
}
