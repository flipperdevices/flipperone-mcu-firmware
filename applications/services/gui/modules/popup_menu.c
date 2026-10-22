#include "popup_menu.h"
#include <m-array.h>
#include <gui/clay_helper.h>
#include <gui/gui.h>
#include <gui/gui_i.h>
#include <gui/modules/elements.h>
#include <assets.h>

#define TAG "GuiPopupMenu"

#define MENU_ID(x) CLAY_SIDI(CLAY_STRING("PopupMenu"), x)

#define MENU_LINE_HEIGHT 15

struct PopupMenu {
    View* view;
    PopupMenuCallback callback;
    void* context;
};

struct PopupMenuItem {
    char* label;
    size_t id;
};

ARRAY_DEF(PopupMenuItemArray, PopupMenuItem, M_POD_OPLIST); // TODO: dict, use id as key, sort by id

typedef struct {
    char* title;
    PopupMenuItemArray_t items;
    size_t position;
    bool visible;
    bool power_pressed;
} PopupMenuViewModel;

static void popup_menu_draw_item(PopupMenuItem* item, size_t line_index, bool selected) {
    CLAY(
        MENU_ID(line_index),
        {
            .layout =
                {
                    .sizing = {.width = CLAY_SIZING_GROW(60.f), .height = CLAY_SIZING_FIXED(MENU_LINE_HEIGHT)},
                    .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                    .layoutDirection = CLAY_LEFT_TO_RIGHT,
                    .padding = {.left = 4, .right = 4, .top = 2, .bottom = 0},
                },
        }) {
        CLAY_TEXT(
            clay_helper_string_from_chars(item->label),
            CLAY_TEXT_CONFIG({
                .fontId = FontBody,
                .textColor = COLOR_BLACK,
                .wrapMode = CLAY_TEXT_WRAP_NONE,
            }));

        if(selected) {
            /* Explicit, item-independent IDs: only one popup item can ever be
             * selected at a time, mirroring the same fix applied to menu.c. */
            CLAY(
                CLAY_ID("PopupMenuSelectionBorder"),
                {
                    .layout = {.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(MENU_LINE_HEIGHT + 1)}},
                    .floating =
                        {
                            .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_TOP, .parent = CLAY_ATTACH_POINT_CENTER_TOP},
                            .attachTo = CLAY_ATTACH_TO_PARENT,
                        },
                    .border =
                        {
                            .color = COLOR_BLACK,
                            .width = {.bottom = 2, .top = 1},
                        },
                }){};

            const Image* left_border = &popup_menu_border_left;
            const Image* right_border = &popup_menu_border_right;

            CLAY(
                CLAY_ID("PopupMenuSelectionLeftCorner"),
                {
                    .layout = {.sizing = {.height = CLAY_SIZING_FIXED(left_border->height), .width = CLAY_SIZING_FIXED(left_border->width)}},
                    .floating =
                        {
                            .attachPoints = {.element = CLAY_ATTACH_POINT_RIGHT_TOP, .parent = CLAY_ATTACH_POINT_LEFT_TOP},
                            .attachTo = CLAY_ATTACH_TO_PARENT,
                        },
                    .image = {.imageData = (void*)left_border},
                }){};
            CLAY(
                CLAY_ID("PopupMenuSelectionRightCorner"),
                {
                    .layout = {.sizing = {.height = CLAY_SIZING_FIXED(right_border->height), .width = CLAY_SIZING_FIXED(right_border->width)}},
                    .floating =
                        {
                            .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_TOP, .parent = CLAY_ATTACH_POINT_RIGHT_TOP},
                            .attachTo = CLAY_ATTACH_TO_PARENT,
                        },
                    .image = {.imageData = (void*)right_border},
                }){};
        }
    }
}

static void popup_menu_draw_item_list(PopupMenuViewModel* model) {
    CLAY(
        CLAY_ID("PopupMenuItems"),
        {
            .layout =
                {
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                    .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                    .childGap = 1,
                },
            .clip = {.vertical = true, .childOffset = Clay_GetScrollOffset()},
        }) {
        size_t line_index = 0;
        PopupMenuItemArray_it_t it;
        for(PopupMenuItemArray_it(it, model->items); !PopupMenuItemArray_end_p(it); PopupMenuItemArray_next(it)) {
            PopupMenuItem* item = PopupMenuItemArray_ref(it);
            bool selected = (line_index == model->position);
            popup_menu_draw_item(item, line_index, selected);

            line_index++;
        }
    }
}

static void popup_menu_draw_title(const char* title) {
    if(title) {
        CLAY(
            CLAY_APP_ID("Title"),
            {
                .backgroundColor = (Clay_Color){0xFF, 0xFF, 0xFF, 0xFF / 2},
                .layout =
                    {
                        .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                        .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                        .padding = {.left = 8, .right = 7, .top = 1, .bottom = 0},
                    },
            }) {
            CLAY_TEXT(clay_helper_string_from_chars(title), CLAY_TEXT_CONFIG({.fontId = FontBig, .textColor = COLOR_BLACK}));
        }
    }
}

static bool popup_menu_layout_callback(void* _model) {
    PopupMenuViewModel* model = _model;
    furi_assert(model);

    if(model->visible == false) {
        return false;
    }

    CLAY(
        CLAY_APP_ID("Overlay"),
        {
            .backgroundColor = (Clay_Color){0xFF, 0xFF, 0xFF, 0xFF / 2},
            .layout =
                {
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                    .childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_TOP},
                },
            .floating =
                {
                    .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_TOP, .parent = CLAY_ATTACH_POINT_LEFT_TOP},
                    .attachTo = CLAY_ATTACH_TO_ROOT,
                },
        }) {
        CLAY(
            CLAY_APP_ID("Dialog"),
            {
                .backgroundColor = COLOR_WHITE,
                .layout =
                    {
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        .sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0)},
                        .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                        .padding = {.left = 7, .right = 7, .top = 3, .bottom = 3},
                        .childGap = 4,
                    },
                .floating =
                    {
                        .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER, .parent = CLAY_ATTACH_POINT_CENTER_CENTER},
                        .attachTo = CLAY_ATTACH_TO_ROOT,
                    },
                .border = {.color = COLOR_BLACK, .width = {.top = 1, .left = 1, .right = 1, .bottom = 1}},
                .cornerRadius = CLAY_CORNER_RADIUS(5),
            }) {
            popup_menu_draw_title(model->title);
            popup_menu_draw_item_list(model);
        }
    }
    elements_softkey_button_element(2, "Power", true, model->power_pressed);

    return false;
}

static bool popup_menu_post_layout_callback(void* _model) {
    PopupMenuViewModel* model = _model;
    furi_check(model);

    if(model->visible == false) {
        return false;
    }

    bool need_redraw = false;

    Clay_ElementId scrollable_container = CLAY_ID("PopupMenuItems");
    Clay_ElementId scroll_target = MENU_ID(model->position);

    if(clay_helper_scroll_to_child(scrollable_container, scroll_target, 0, 0, 15)) {
        need_redraw = true;
    }

    return need_redraw;
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
        if(event->type == InputTypePress && event->key == InputKeyPower) {
            with_view_model(
                menu->view,
                PopupMenuViewModel * model,
                {
                    model->visible = true;
                    model->power_pressed = true;
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
    } else if(event->type == InputTypePress && (event->key == InputKeyPower || event->key == InputKeyBack)) {
        with_view_model(menu->view, PopupMenuViewModel * model, { model->visible = false; }, true);
        if(menu->callback) {
            menu->callback(POPUP_MENU_EXIT_ID, menu->context);
        }
        consumed = true;
    } else if(event->type == InputTypeRelease && event->key == InputKeyPower) {
        with_view_model(menu->view, PopupMenuViewModel * model, { model->power_pressed = false; }, true);
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

    view_allocate_model(menu->view, ViewModelTypeLockFree, sizeof(PopupMenuViewModel));
    with_view_model(
        menu->view,
        PopupMenuViewModel * model,
        {
            PopupMenuItemArray_init(model->items);
            model->power_pressed = true;
        },
        false);

    view_set_layout_callback(menu->view, popup_menu_layout_callback);
    view_set_post_layout_callback(menu->view, popup_menu_post_layout_callback);
    view_set_input_callback(menu->view, popup_menu_input_callback, menu);
    view_set_transparent(menu->view, true);

    return menu;
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
    view_set_post_layout_callback(menu->view, NULL);
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
            model->power_pressed = true;
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
    PopupMenuItem* item = NULL;

    with_view_model(
        menu->view,
        PopupMenuViewModel * model,
        {
            item = PopupMenuItemArray_push_new(model->items);

            item->label = strdup(label);
            item->id = id;
        },
        true);
}
