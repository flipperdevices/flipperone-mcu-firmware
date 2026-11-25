#include <assets.h>
#include <furi.h>
#include <gui/clay_helper.h>
#include <gui/modules/elements.h>
#include <stdio.h>
#include <math.h>

/*
 * Battery gauge, after the mock-up: inside the 41x17 outline the fill is a
 * rounded bar starting 2 px in from the left, up to 34 px wide (1 px corner
 * cut on its left end, and on its right end only when full). The light look
 * centres the percentage over the body and draws it white where it lies on
 * the fill; the dark look centres the charging bolt over the body instead.
 */
#define BATTERY_FILL_X        2
#define BATTERY_FILL_Y        2
#define BATTERY_FILL_WIDTH    34
#define BATTERY_FILL_HEIGHT   13
#define BATTERY_TEXT_CENTER_X 19
#define BATTERY_TEXT_Y        5
#define BATTERY_BOLT_X        15
#define BATTERY_BOLT_Y        3

static void elements_battery_render(Canvas* canvas, const Clay_BoundingBox* bb, void* context) {
    ElementsBattery* battery = context;
    int32_t x = bb->x;
    int32_t y = bb->y;
    const bool dark = (battery->look != ElementsBatteryLookLight);
    const ColorA ink = {.color = dark ? 0xFF : 0x00, .alpha = 255};
    const ColorA paper = {.color = dark ? 0x00 : 0xFF, .alpha = 255};

    canvas_draw_image(canvas, x, y, dark ? &header_battery_black : &header_battery_idle, false);

    /* Any charge at all shows at least one column. */
    int32_t fill = (int32_t)ceilf(BATTERY_FILL_WIDTH * battery->percent / 100.f);
    int32_t fill_x = x + BATTERY_FILL_X;
    int32_t fill_y = y + BATTERY_FILL_Y;
    if(fill > 0) {
        render_fill_round_rectangle(canvas, fill_x, fill_y + 1, fill, BATTERY_FILL_HEIGHT - 2, 0, ink);
        int32_t inner = fill - 1 - (fill == BATTERY_FILL_WIDTH ? 1 : 0);
        if(inner > 0) render_fill_round_rectangle(canvas, fill_x + 1, fill_y, inner, BATTERY_FILL_HEIGHT, 0, ink);
    }

    if(dark) {
        if(battery->look == ElementsBatteryLookDark) {
            canvas_draw_image(canvas, x + BATTERY_BOLT_X, y + BATTERY_BOLT_Y, &battery_charging_bolt, false);
        }
        return;
    }

    int32_t text_x = x + BATTERY_TEXT_CENTER_X - (int32_t)canvas_text_width(FontBody, battery->text) / 2;
    int32_t text_y = y + BATTERY_TEXT_Y;
    int32_t split_x = fill_x + fill;
    CanvasScissor saved;

    canvas_scissor_intersect(canvas, x, y, split_x - x, header_battery_idle.height, &saved);
    canvas_draw_text(canvas, text_x, text_y, FontBody, paper, battery->text);
    canvas_scissor_restore(canvas, &saved);

    canvas_scissor_intersect(canvas, split_x, y, x + header_battery_idle.width - split_x, header_battery_idle.height, &saved);
    canvas_draw_text(canvas, text_x, text_y, FontBody, ink, battery->text);
    canvas_scissor_restore(canvas, &saved);
}

void elements_battery_init(ElementsBattery* battery) {
    furi_check(battery);
    battery->element.render = elements_battery_render;
    battery->element.context = battery;
    battery->percent = 0.f;
    battery->look = ElementsBatteryLookLight;
    battery->text[0] = '\0';
}

void elements_battery_element(Clay_ElementId id, ElementsBattery* battery, float percent, ElementsBatteryLook look) {
    furi_check(battery && battery->element.render);
    battery->percent = fminf(fmaxf(percent, 0.f), 100.f);
    battery->look = look;
    snprintf(battery->text, sizeof(battery->text), "%u", (unsigned)(battery->percent + 0.5f));

    const Image* image = (look != ElementsBatteryLookLight) ? &header_battery_black : &header_battery_idle;
    CLAY(
        id,
        {
            .layout =
                {
                    .sizing = {.width = CLAY_SIZING_FIXED(image->width), .height = CLAY_SIZING_FIXED(image->height)},
                },
            .custom = {.customData = &battery->element},
        }) {
    }
}

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
                    .zIndex = active ? 100 : 0,
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
 * Home screen tabs along the bottom edge, laid out after the mock-up: Help (1), Power (2) and Settings (3) around the centre,
 * with 0 and 4 continuing the row outwards. The tab art is a PNG with
 * transparent corners; Settings-side tabs mirror the Help one. Positions are
 * in the 256 px wide mock-up, shifted by one to centre on the 258 px display.
 */

#define HOME_TAB_COUNT 5
#define HOME_TAB_POWER_INDEX 2
#define HOME_TAB_SCREEN_X_OFFSET 1

/* Left edge of each tab in the mock-up. */
static const int16_t home_tab_x[HOME_TAB_COUNT] = {2, 52, 99, 154, 206};

/* The label sits 3 px below the tab's top edge; the text box starts 1 px
 * above its glyphs. */
#define HOME_TAB_LABEL_PADDING_TOP 4
/* Slanted tabs look centred with the label nudged away from the slant. */
#define HOME_TAB_LABEL_SLANT_NUDGE 4
/* Power: the icon's top is 13 px below the tab's top, so the label's box is
 * cut to that height (the glyphs themselves are only 7 px tall). */
#define HOME_TAB_POWER_LABEL_HEIGHT 9

void elements_home_tab_element(size_t index, const char* text, bool pressed) {
    furi_check(text);
    furi_check(index < HOME_TAB_COUNT);

    const bool inverted = pressed;
    const bool power = (index == HOME_TAB_POWER_INDEX);
    const bool mirrored = (index > HOME_TAB_POWER_INDEX);

    const Image* image;
    if(power) {
        image = inverted ? &desktop_button_power_pressed : &desktop_button_power_inactive;
    } else if(mirrored) {
        image = inverted ? &desktop_button_settings_pressed : &desktop_button_settings_inactive;
    } else {
        image = inverted ? &desktop_button_help_pressed : &desktop_button_help_inactive;
    }

    const Clay_Sizing sizing = {.width = CLAY_SIZING_FIXED(image->width), .height = CLAY_SIZING_FIXED(image->height)};
    const Clay_Color text_color = inverted ? COLOR_WHITE : COLOR_BLACK;
    Clay_TextElementConfig* text_config = CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = text_color, .wrapMode = CLAY_TEXT_WRAP_NONE});

    Clay_Padding padding = {.top = HOME_TAB_LABEL_PADDING_TOP};
    if(!power) {
        if(mirrored) {
            padding.left = HOME_TAB_LABEL_SLANT_NUDGE;
        } else {
            padding.right = HOME_TAB_LABEL_SLANT_NUDGE;
        }
    }

    CLAY(
        CLAY_IDI_LOCAL("HomeTab", index),
        {
            .layout =
                {
                    .sizing = sizing,
                },
            .floating =
                {
                    .offset = {.x = home_tab_x[index] + HOME_TAB_SCREEN_X_OFFSET, .y = 0},
                    .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_BOTTOM, .parent = CLAY_ATTACH_POINT_LEFT_BOTTOM},
                    .attachTo = CLAY_ATTACH_TO_ROOT,
                    /* The Power tab overlaps its neighbours' slanted sides and goes on top. */
                    .zIndex = power ? 1 : 0,
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
                CLAY(
                    CLAY_IDI_LOCAL("HomeTabLabel", index),
                    {
                        .layout = {.sizing = {.height = CLAY_SIZING_FIXED(HOME_TAB_POWER_LABEL_HEIGHT)}},
                    }) {
                    CLAY_TEXT(clay_helper_string_from_chars(text), text_config);
                }
                clay_fixed_image(CLAY_IDI_LOCAL("HomeTabIcon", index), inverted ? &desktop_button_power_icon_pressed : &desktop_button_power_icon);
            } else {
                CLAY_TEXT(clay_helper_string_from_chars(text), text_config);
            }
        }
    }
}
