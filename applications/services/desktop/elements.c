#include <assets.h>
#include <furi.h>
#include <gui/clay_helper.h>
#include "elements.h"

void elements_softkey_button_element(size_t index, const char* text, bool active, bool pressed) {
    furi_check(text);
    furi_check(index < 5);

    const Image* image;

    if(pressed) {
        image = &button_pressed;
    } else if(active) {
        image = &button_active;
    } else {
        image = &button_inactive;
    }

    const Clay_Sizing sizing = {.width = CLAY_SIZING_FIXED(image->width), .height = CLAY_SIZING_FIXED(image->height)};

    const float offset_x = ((float)index - 2.0f) * (image->width + 4);

    CLAY_AUTO_ID({
        .layout =
            {
                .sizing = sizing,

            },
        .floating =
            {
                .offset = {.x = offset_x, .y = 0},
                .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_BOTTOM, .parent = CLAY_ATTACH_POINT_CENTER_BOTTOM},
                .attachTo = CLAY_ATTACH_TO_ROOT,
                .zIndex = active ? 100 : 0,
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
            CLAY_TEXT(clay_helper_string_from_chars(text), CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = pressed ? COLOR_WHITE : COLOR_BLACK}));
        }
    }
}
