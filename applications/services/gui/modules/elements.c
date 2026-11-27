#include <assets.h>
#include <furi.h>
#include <gui/clay_helper.h>
#include <gui/modules/elements.h>

void elements_softkey_button_element(size_t index, const char* text, bool active, bool pressed) {
    furi_check(text);
    furi_check(index < 5);

    const Image* image;

    if(pressed) {
        image = &desktop_button_pressed;
    } else if(active) {
        image = &desktop_button_active;
    } else {
        image = &desktop_button_inactive;
    }

    const Clay_Sizing sizing = {.width = CLAY_SIZING_FIXED(image->width), .height = CLAY_SIZING_FIXED(image->height)};

    const float offset_x = ((float)index - 2.0f) * (image->width + 4);

    CLAY(
        CLAY_IDI_LOCAL("SoftkeyButton", index),
        {
            .layout =
                {
                    .sizing = sizing,

                },
            .floating =
                {
                    .offset = {.x = offset_x, .y = 0},
                    .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_BOTTOM, .parent = CLAY_ATTACH_POINT_CENTER_BOTTOM},
                    .attachTo = CLAY_ATTACH_TO_ROOT,
                    // Active: the key of an open popup menu, over it (POPUP_MENU_Z_INDEX 1000)
                    .zIndex = active ? 1100 : 0,
                },
        }) {
        CLAY(
            CLAY_IDI_LOCAL("SoftkeyButtonImage", index),
            {
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

/*
 * Home screen tabs along the bottom edge, after the mock-up: Help (1), Power (2)
 * and Settings (3) around the centre, 0 and 4 continuing the row outwards.
 * The tab art has transparent corners; Settings-side tabs mirror the Help one.
 * Positions are in the 256 px wide mock-up, shifted by one onto the 258 px display.
 */

#define HOME_TAB_COUNT           5
#define HOME_TAB_POWER_INDEX     2
#define HOME_TAB_SCREEN_X_OFFSET 1

// Left edge of each tab in the mock-up
static const int16_t home_tab_x[HOME_TAB_COUNT] = {2, 52, 99, 154, 206};

// Labels as the mock places them: glyphs 4 px below a side tab's top edge,
// 3 px below Power's; a side tab's label nudged away from its slant
#define HOME_TAB_LABEL_PADDING_TOP       4
#define HOME_TAB_POWER_LABEL_PADDING_TOP 3
#define HOME_TAB_LABEL_NUDGE_LEFT        4
#define HOME_TAB_LABEL_NUDGE_RIGHT       5
// Power: the icon's top is 13 px below the tab's top, so the label's box ends
// there, and the icon sits a pixel right of the centre
#define HOME_TAB_POWER_LABEL_HEIGHT      (13 - HOME_TAB_POWER_LABEL_PADDING_TOP)
#define HOME_TAB_POWER_ICON_NUDGE        1

void elements_home_tab_element(size_t index, const char* text, bool active, bool pressed) {
    furi_check(text);
    furi_check(index < HOME_TAB_COUNT);

    const bool power = (index == HOME_TAB_POWER_INDEX);
    const bool mirrored = (index > HOME_TAB_POWER_INDEX);

    const Image* image;
    if(power) {
        image = pressed ? &desktop_button_power_pressed : &desktop_button_power_inactive;
    } else if(mirrored) {
        image = pressed ? &desktop_button_settings_pressed : &desktop_button_settings_inactive;
    } else {
        image = pressed ? &desktop_button_help_pressed : &desktop_button_help_inactive;
    }

    const Clay_Sizing sizing = {.width = CLAY_SIZING_FIXED(image->width), .height = CLAY_SIZING_FIXED(image->height)};
    Clay_TextElementConfig* text_config =
        CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = pressed ? COLOR_WHITE : COLOR_BLACK, .wrapMode = CLAY_TEXT_WRAP_NONE});

    Clay_Padding padding = {.top = power ? HOME_TAB_POWER_LABEL_PADDING_TOP : HOME_TAB_LABEL_PADDING_TOP};
    if(!power) {
        if(mirrored) {
            padding.left = HOME_TAB_LABEL_NUDGE_RIGHT;
        } else {
            padding.right = HOME_TAB_LABEL_NUDGE_LEFT;
        }
    }

    CLAY(
        CLAY_IDI_LOCAL("HomeTab", index),
        {
            .layout = {.sizing = sizing},
            .floating =
                {
                    .offset = {.x = home_tab_x[index] + HOME_TAB_SCREEN_X_OFFSET, .y = 0},
                    .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_BOTTOM, .parent = CLAY_ATTACH_POINT_LEFT_BOTTOM},
                    .attachTo = CLAY_ATTACH_TO_ROOT,
                    // Active: the key of an open popup menu, over it (POPUP_MENU_Z_INDEX 1000).
                    // Power overlaps its neighbours' slanted sides.
                    .zIndex = active ? 1100 :
                              power  ? 1 :
                                       0,
                },
        }) {
        CLAY(
            CLAY_IDI_LOCAL("HomeTabImage", index),
            {
                .layout =
                    {
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        .sizing = sizing,
                        .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_TOP},
                        .padding = padding,
                    },
                .image = {.imageData = (void*)image},
            }) {
            if(power) {
                CLAY(CLAY_IDI_LOCAL("HomeTabLabel", index), {.layout = {.sizing = {.height = CLAY_SIZING_FIXED(HOME_TAB_POWER_LABEL_HEIGHT)}}}) {
                    CLAY_TEXT(clay_helper_string_from_chars(text), text_config);
                }
                CLAY(CLAY_IDI_LOCAL("HomeTabIconBox", index), {.layout = {.padding = {.left = HOME_TAB_POWER_ICON_NUDGE}}}) {
                    clay_fixed_image(CLAY_IDI_LOCAL("HomeTabIcon", index), pressed ? &desktop_button_power_icon_pressed : &desktop_button_power_icon);
                }
            } else {
                CLAY_TEXT(clay_helper_string_from_chars(text), text_config);
            }
        }
    }
}
