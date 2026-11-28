#include "popup_menu.h"
#include <m-array.h>
#include <gui/clay_helper.h>
#include <gui/gui.h>
#include <gui/gui_i.h>
#include <gui/modules/elements.h>

#define TAG "GuiPopupMenu"

#define MENU_ID(x) CLAY_SIDI(CLAY_STRING("PopupMenu"), x)

/* The window from the mock, in pixels from its top left corner: a white box
 * with a black border, corners cut by 3 pixels, a gray header and the same
 * shape in black 3 pixels down and right as its shadow. It is centered on the
 * screen. */
#define POPUP_MENU_CORNER        3
#define POPUP_MENU_SHADOW        3
#define POPUP_MENU_HEADER_BOTTOM 21 // The header is gray from below the border to here
#define POPUP_MENU_ICON_X        5
#define POPUP_MENU_ICON_Y        4
#define POPUP_MENU_TITLE_Y       7
#define POPUP_MENU_LINES_X       19
#define POPUP_MENU_LINES_Y       25

/* A line of the list and its selector: the selector is 17 pixels tall with
 * its shadow, left aligned lines overlap by that shadow */
#define POPUP_MENU_SELECTOR_HEIGHT 17

#define POPUP_MENU_HEADER_COLOR 0xE4

typedef struct {
    uint8_t title_x;
    uint8_t line_height;
    uint8_t bottom; // From the last line to the bottom border, the border included
    uint8_t label_x; // Left aligned only, centered labels have none
    uint8_t label_right; // Moves the labels of unselected centered lines left by half of it
    uint8_t label_y;
    uint8_t selected_label_y;
    uint8_t selector_extra; // The selector is this much wider than the widest selected label
} PopupMenuAlignLayout;

static const PopupMenuAlignLayout popup_menu_align_layouts[] = {
    [PopupMenuAlignCenter] =
        {
            .title_x = 22,
            .line_height = POPUP_MENU_SELECTOR_HEIGHT,
            .bottom = 4,
            .label_right = 2,
            .label_y = 4,
            .selected_label_y = 4,
            .selector_extra = 38,
        },
    [PopupMenuAlignLeft] =
        {
            .title_x = 24,
            .line_height = POPUP_MENU_SELECTOR_HEIGHT - 1,
            .bottom = 5,
            .label_x = 10,
            .label_y = 5,
            .selected_label_y = 4,
            .selector_extra = 31,
        },
};

static const ColorA POPUP_MENU_BLACK = {.color = 0x00, .alpha = 0xFF};
static const ColorA POPUP_MENU_WHITE = {.color = 0xFF, .alpha = 0xFF};
static const ColorA POPUP_MENU_GRAY = {.color = POPUP_MENU_HEADER_COLOR, .alpha = 0xFF};

struct PopupMenu {
    View* view;
    PopupMenuCallback callback;
    void* context;
    InputKey key;
};

struct PopupMenuItem {
    char* label;
    size_t id;
};

ARRAY_DEF(PopupMenuItemArray, PopupMenuItem, M_POD_OPLIST); // TODO: dict, use id as key, sort by id

typedef struct {
    char* title;
    const Image* icon;
    PopupMenuAlign align;
    PopupMenuItemArray_t items;
    // The widest label in the selected line's font: the selector and the window fit it
    uint16_t selected_label_width;
    size_t position;
    bool visible;
    bool key_pressed;
    PopupMenuSoftkeyStyle softkey_style;
    size_t softkey_index;
    const char* softkey_label;
    ClayCustomElement window;
    ClayCustomElement selector;
} PopupMenuViewModel;

// Row y of a shape whose corners are cut by `corner` pixels: x0 and x1 of its edge
static void popup_menu_cut_row(int32_t width, int32_t height, int32_t corner, int32_t row, int32_t* x0, int32_t* x1) {
    int32_t inset = 0;
    if(row < corner) {
        inset = corner - row;
    } else if(row >= height - corner) {
        inset = corner - (height - 1 - row);
    }
    *x0 = inset;
    *x1 = width - 1 - inset;
}

static void popup_menu_fill_cut_box(Canvas* canvas, int32_t x, int32_t y, int32_t width, int32_t height, ColorA color) {
    for(int32_t row = 0; row < height; row++) {
        int32_t x0, x1;
        popup_menu_cut_row(width, height, POPUP_MENU_CORNER, row, &x0, &x1);
        render_draw_line(canvas, x + x0, y + row, x + x1, y + row, color);
    }
}

static void popup_menu_render_window(Canvas* canvas, const Clay_BoundingBox* box, void* context) {
    UNUSED(context);
    const int32_t x = box->x;
    const int32_t y = box->y;
    const int32_t width = box->width - POPUP_MENU_SHADOW;
    const int32_t height = box->height - POPUP_MENU_SHADOW;

    popup_menu_fill_cut_box(canvas, x + POPUP_MENU_SHADOW, y + POPUP_MENU_SHADOW, width, height, POPUP_MENU_BLACK);
    popup_menu_fill_cut_box(canvas, x, y, width, height, POPUP_MENU_BLACK);

    // The inside: the header, then white
    for(int32_t row = 1; row < height - 1; row++) {
        int32_t x0, x1;
        popup_menu_cut_row(width, height, POPUP_MENU_CORNER, row, &x0, &x1);
        const ColorA color = row < POPUP_MENU_HEADER_BOTTOM ? POPUP_MENU_GRAY : POPUP_MENU_WHITE;
        render_draw_line(canvas, x + x0 + 1, y + row, x + x1 - 1, y + row, color);
    }
}

// An outline with corners cut by 2 pixels and a 1 pixel shadow right and below
static void popup_menu_render_selector(Canvas* canvas, const Clay_BoundingBox* box, void* context) {
    UNUSED(context);
    const int32_t x = box->x;
    const int32_t y = box->y;
    const int32_t r = x + box->width - 1;
    const int32_t b = y + box->height - 1;

    render_draw_line(canvas, x + 2, y, r - 3, y, POPUP_MENU_BLACK);
    render_draw_line(canvas, x + 1, y + 1, x + 1, y + 1, POPUP_MENU_BLACK);
    render_draw_line(canvas, r - 2, y + 1, r - 2, y + 1, POPUP_MENU_BLACK);
    render_draw_line(canvas, x, y + 2, x, b - 3, POPUP_MENU_BLACK);
    render_draw_line(canvas, r - 1, y + 2, r - 1, b - 3, POPUP_MENU_BLACK);
    render_draw_line(canvas, r, y + 3, r, b - 3, POPUP_MENU_BLACK);
    render_draw_line(canvas, x + 1, b - 2, x + 1, b - 2, POPUP_MENU_BLACK);
    render_draw_line(canvas, r - 2, b - 2, r - 1, b - 2, POPUP_MENU_BLACK);
    render_draw_line(canvas, x + 2, b - 1, r - 2, b - 1, POPUP_MENU_BLACK);
    render_draw_line(canvas, x + 3, b, r - 3, b, POPUP_MENU_BLACK);
}

static uint16_t popup_menu_selector_width(const PopupMenuViewModel* model) {
    return model->selected_label_width + popup_menu_align_layouts[model->align].selector_extra;
}

static void popup_menu_draw_item(PopupMenuViewModel* model, PopupMenuItem* item, size_t line_index, bool selected) {
    const PopupMenuAlignLayout* layout = &popup_menu_align_layouts[model->align];
    const bool left = model->align == PopupMenuAlignLeft;

    CLAY(
        MENU_ID(line_index),
        {
            .layout =
                {
                    .sizing = {.width = CLAY_SIZING_FIXED(popup_menu_selector_width(model)), .height = CLAY_SIZING_FIXED(layout->line_height)},
                    .childAlignment = {.x = left ? CLAY_ALIGN_X_LEFT : CLAY_ALIGN_X_CENTER},
                    .padding =
                        {
                            .left = layout->label_x,
                            .right = selected ? 0 : layout->label_right,
                            .top = selected ? layout->selected_label_y : layout->label_y,
                        },
                },
        }) {
        CLAY_TEXT(
            clay_helper_string_from_chars(item->label),
            CLAY_TEXT_CONFIG({
                .fontId = selected ? FontButton : FontBody,
                .textColor = COLOR_BLACK,
                .wrapMode = CLAY_TEXT_WRAP_NONE,
            }));

        if(selected) {
            // Floating: in left aligned lists it reaches into the next line
            CLAY(
                CLAY_ID("PopupMenuSelector"),
                {
                    .layout = {.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(POPUP_MENU_SELECTOR_HEIGHT)}},
                    .floating =
                        {
                            .attachTo = CLAY_ATTACH_TO_PARENT,
                            .zIndex = POPUP_MENU_Z_INDEX + 1,
                        },
                    .custom = {.customData = &model->selector},
                }){};
        }
    }
}

static void popup_menu_draw_header(PopupMenuViewModel* model) {
    if(model->icon) {
        CLAY(
            CLAY_ID("PopupMenuIcon"),
            {
                .layout = {.sizing = {.width = CLAY_SIZING_FIXED(model->icon->width), .height = CLAY_SIZING_FIXED(model->icon->height)}},
                .floating = {.offset = {POPUP_MENU_ICON_X, POPUP_MENU_ICON_Y}, .attachTo = CLAY_ATTACH_TO_PARENT, .zIndex = POPUP_MENU_Z_INDEX},
                .image = {.imageData = (void*)model->icon},
            }){};
    }
    if(model->title) {
        CLAY(
            CLAY_ID("PopupMenuTitle"),
            {
                .floating =
                    {
                        .offset = {popup_menu_align_layouts[model->align].title_x, POPUP_MENU_TITLE_Y},
                        .attachTo = CLAY_ATTACH_TO_PARENT,
                        .zIndex = POPUP_MENU_Z_INDEX,
                    },
            }) {
            CLAY_TEXT(
                clay_helper_string_from_chars(model->title), CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = COLOR_BLACK, .wrapMode = CLAY_TEXT_WRAP_NONE}));
        }
    }
}

static bool popup_menu_layout_callback(void* _model) {
    PopupMenuViewModel* model = _model;
    furi_assert(model);

    if(model->visible == false) {
        return false;
    }

    const PopupMenuAlignLayout* layout = &popup_menu_align_layouts[model->align];
    // Centered without its shadow, on whole pixels of the screen with even sides
    const size_t lines = PopupMenuItemArray_size(model->items);
    const uint16_t width = POPUP_MENU_LINES_X * 2 + popup_menu_selector_width(model);
    const uint16_t height = POPUP_MENU_LINES_Y + lines * layout->line_height + (POPUP_MENU_SELECTOR_HEIGHT - layout->line_height) + layout->bottom;
    const float x = (POPUP_MENU_SHADOW - width % 2) / 2.0f;
    const float y = (POPUP_MENU_SHADOW - height % 2) / 2.0f;

    CLAY(
        CLAY_APP_ID("Overlay"),
        {
            .backgroundColor = (Clay_Color){0xFF, 0xFF, 0xFF, 0xFF / 2},
            .layout = {.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)}},
            .floating = {.attachTo = CLAY_ATTACH_TO_ROOT, .zIndex = POPUP_MENU_Z_INDEX},
        }) {
        CLAY(
            CLAY_APP_ID("Window"),
            {
                .layout =
                    {
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        .padding =
                            {
                                .left = POPUP_MENU_LINES_X,
                                .right = POPUP_MENU_LINES_X + POPUP_MENU_SHADOW,
                                .top = POPUP_MENU_LINES_Y,
                                .bottom = layout->bottom + POPUP_MENU_SHADOW,
                            },
                    },
                .floating =
                    {
                        .offset = {x, y},
                        .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER, .parent = CLAY_ATTACH_POINT_CENTER_CENTER},
                        .attachTo = CLAY_ATTACH_TO_ROOT,
                        .zIndex = POPUP_MENU_Z_INDEX,
                    },
                .custom = {.customData = &model->window},
            }) {
            popup_menu_draw_header(model);

            size_t line_index = 0;
            PopupMenuItemArray_it_t it;
            for(PopupMenuItemArray_it(it, model->items); !PopupMenuItemArray_end_p(it); PopupMenuItemArray_next(it)) {
                popup_menu_draw_item(model, PopupMenuItemArray_ref(it), line_index, line_index == model->position);
                line_index++;
            }
            if(model->align == PopupMenuAlignLeft) {
                // The last selector's shadow
                CLAY(CLAY_ID("PopupMenuLastShadow"), {.layout = {.sizing = {.height = CLAY_SIZING_FIXED(1)}}}){};
            }
        }
    }
    if(model->softkey_style == PopupMenuSoftkeyStyleHomeTab) {
        elements_home_tab_element(model->softkey_index, model->softkey_label, true, model->key_pressed);
    } else {
        elements_softkey_button_element(model->softkey_index, model->softkey_label, true, model->key_pressed);
    }

    return false;
}

static void popup_menu_process_up_down(PopupMenu* menu, int8_t delta) {
    with_view_model(
        menu->view,
        PopupMenuViewModel * model,
        {
            size_t items_count = PopupMenuItemArray_size(model->items);
            if(items_count > 0) {
                size_t new_position = (model->position + delta + items_count) % items_count;
                model->position = new_position;
            }
        },
        true);
}

static bool popup_menu_input_callback(InputEvent* event, void* context) {
    furi_check(context);
    PopupMenu* menu = context;
    bool consumed = false;

    bool visible = false;
    with_view_model(menu->view, PopupMenuViewModel * model, { visible = model->visible; }, false);
    if(!visible) {
        if(event->type == InputTypePress && event->key == menu->key) {
            with_view_model(
                menu->view,
                PopupMenuViewModel * model,
                {
                    model->visible = true;
                    model->key_pressed = true;
                },
                true);
            consumed = true;
        }

        return consumed;
    }

    if(event->type == InputTypePress && event->key == InputKeyOk) {
        size_t selected_id = 0;
        PopupMenuItem* selected_item = NULL;
        with_view_model(
            menu->view,
            PopupMenuViewModel * model,
            {
                selected_item = PopupMenuItemArray_get(model->items, model->position);
                furi_check(selected_item);
                selected_id = selected_item->id;
            },
            false);
        if(menu->callback) {
            menu->callback(selected_id, menu->context);
        }
        consumed = true;
    } else if(event->type == InputTypePress && (event->key == menu->key || event->key == InputKeyBack)) {
        with_view_model(menu->view, PopupMenuViewModel * model, { model->visible = false; }, true);
        if(menu->callback) {
            menu->callback(POPUP_MENU_EXIT_ID, menu->context);
        }
        consumed = true;
    } else if(event->type == InputTypeRelease && event->key == menu->key) {
        with_view_model(menu->view, PopupMenuViewModel * model, { model->key_pressed = false; }, true);
    } else if(event->type == InputTypePress || event->type == InputTypeRepeat) {
        if(event->key == InputKeyUp || event->key == InputKeyDown) {
            popup_menu_process_up_down(menu, event->key == InputKeyUp ? -1 : 1);
            consumed = true;
        }
    }

    consumed = true; // Consume all input events when the popup menu is visible

    return consumed;
}

PopupMenu* popup_menu_alloc(View* view) {
    furi_check(view);
    PopupMenu* menu = malloc(sizeof(PopupMenu));
    menu->view = view;
    menu->key = InputKeyPower;

    view_allocate_model(menu->view, ViewModelTypeLocking, sizeof(PopupMenuViewModel));
    with_view_model(
        menu->view,
        PopupMenuViewModel * model,
        {
            PopupMenuItemArray_init(model->items);
            model->key_pressed = true;
            model->softkey_index = 2;
            model->softkey_label = "Power";
            model->window.render = popup_menu_render_window;
            model->selector.render = popup_menu_render_selector;
        },
        false);

    view_set_layout_callback(menu->view, popup_menu_layout_callback);
    view_set_input_callback(menu->view, popup_menu_input_callback, menu);
    view_set_transparent(menu->view, true);

    return menu;
}

void popup_menu_set_softkey(PopupMenu* menu, PopupMenuSoftkeyStyle style, size_t index, const char* label, InputKey key) {
    furi_check(menu);
    furi_check(label);
    menu->key = key;
    with_view_model(
        menu->view,
        PopupMenuViewModel * model,
        {
            model->softkey_label = label;
            model->softkey_style = style;
            model->softkey_index = index;
        },
        true);
}

void popup_menu_free(PopupMenu* menu) {
    furi_check(menu);
    menu->callback = NULL;
    with_view_model(
        menu->view,
        PopupMenuViewModel * model,
        {
            if(model->title) free(model->title);
            PopupMenuItemArray_it_t it;
            for(PopupMenuItemArray_it(it, model->items); !PopupMenuItemArray_end_p(it); PopupMenuItemArray_next(it)) {
                PopupMenuItem* item = PopupMenuItemArray_ref(it);

                if(item->label) free(item->label);
            }
            PopupMenuItemArray_clear(model->items);
        },
        false);
    view_set_layout_callback(menu->view, NULL);
    view_set_input_callback(menu->view, NULL, NULL);
    view_free_model(menu->view);
    free(menu);
}

void popup_menu_set_title(PopupMenu* menu, const char* title) {
    with_view_model(
        menu->view,
        PopupMenuViewModel * model,
        {
            if(model->title) free(model->title);
            model->title = title ? strdup(title) : NULL;
        },
        true);
}

void popup_menu_set_icon(PopupMenu* menu, const Image* icon) {
    furi_check(menu);
    with_view_model(menu->view, PopupMenuViewModel * model, { model->icon = icon; }, true);
}

void popup_menu_set_align(PopupMenu* menu, PopupMenuAlign align) {
    furi_check(menu);
    with_view_model(menu->view, PopupMenuViewModel * model, { model->align = align; }, true);
}

void popup_menu_set_position(PopupMenu* menu, size_t item_id) {
    furi_check(menu);
    with_view_model(
        menu->view,
        PopupMenuViewModel * model,
        {
            size_t position = 0;
            PopupMenuItemArray_it_t it;
            for(PopupMenuItemArray_it(it, model->items); !PopupMenuItemArray_end_p(it); PopupMenuItemArray_next(it)) {
                PopupMenuItem* item = PopupMenuItemArray_ref(it);
                if(item->id == item_id) {
                    model->position = position;
                    break;
                }
                position++;
            }
        },
        true);
}

void popup_menu_set_visible(PopupMenu* menu, bool visible) {
    furi_check(menu);
    with_view_model(
        menu->view,
        PopupMenuViewModel * model,
        {
            model->visible = visible;
            model->key_pressed = true;
        },
        true);
}

bool popup_menu_is_visible(PopupMenu* menu) {
    furi_check(menu);
    bool visible = false;
    with_view_model(menu->view, PopupMenuViewModel * model, { visible = model->visible; }, false);
    return visible;
}

void popup_menu_set_callback(PopupMenu* menu, PopupMenuCallback callback, void* context) {
    furi_check(menu);
    menu->callback = callback;
    menu->context = context;
}

void popup_menu_add_item(PopupMenu* menu, const char* label, size_t id) {
    furi_check(menu);
    furi_check(label);

    // Any line can be selected: measure it in the selected line's font
    Clay_TextElementConfig font = {.fontId = FontButton};
    const Clay_StringSlice slice = {.length = strlen(label), .chars = label, .baseChars = label};
    const uint16_t width = clay_render_measure_text(slice, &font, NULL).width;

    with_view_model(
        menu->view,
        PopupMenuViewModel * model,
        {
            PopupMenuItem* item = PopupMenuItemArray_push_new(model->items);
            item->label = strdup(label);
            item->id = id;
            if(width > model->selected_label_width) model->selected_label_width = width;
        },
        true);
}
