#include "menu.h"
#include <m-array.h>
#include <gui/clay_helper.h>
#include <gui/gui.h>
#include <assets.h>

#define TAG "GuiMenu"

#define MENU_ID(x) CLAY_SIDI(CLAY_STRING("Menu"), x)

#define MENU_LINE_HEIGHT 21

static const Clay_Color COLOR_GRAY = {0xAA, 0xAA, 0xAA, 0xFF};

struct Menu {
    View* view;
    MenuItemCallback callback;
    void* context;
};

struct MenuItem {
    char* label;
    size_t id;
    MenuItemSubType type;
    union {
        char* sub_label;
        struct {
            FuriString* value_text;
            size_t value_index;
            size_t max_index;
            MenuItemSelectorCallback callback;
        } selector;
    };
};

ARRAY_DEF(MenuItemArray, MenuItem, M_POD_OPLIST); // TODO: dict, use id as key, sort by id

typedef struct {
    char* title;
    MenuItemArray_t items;
    size_t position;

    size_t scrollbar_offset;
    float scrollbar_len;
    bool show_scrollbar;
} MenuViewModel;

static void menu_draw_item(MenuItem* item, size_t line_index, bool selected) {
    CLAY(
        MENU_ID(line_index),
        {
            .layout =
                {
                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(MENU_LINE_HEIGHT)},
                    .childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER},
                    .layoutDirection = CLAY_LEFT_TO_RIGHT,
                    .padding = {.left = selected ? 1 : 2, .right = 0, .top = selected ? 2 : 4, .bottom = 0},
                    .childGap = 4,
                },
        }) {
        const char* sub_label = NULL;
        if(item->type == MenuItemSubTypeLabel) {
            sub_label = item->sub_label;
        } else if(item->type == MenuItemSubTypeSelector) {
            sub_label = furi_string_get_cstr(item->selector.value_text);
        }

        CLAY_TEXT(
            clay_helper_string_from_chars(item->label),
            CLAY_TEXT_CONFIG({
                .fontId = selected ? FontBig : FontBusy9,
                .textColor = COLOR_BLACK,
                .wrapMode = CLAY_TEXT_WRAP_NONE,
            }));

        CLAY_AUTO_ID({.layout = {.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)}}}){};

        if(sub_label) {
            CLAY_TEXT(
                clay_helper_string_from_chars(sub_label),
                CLAY_TEXT_CONFIG({
                    .fontId = FontBusy9,
                    .textColor = selected ? COLOR_BLACK : COLOR_GRAY,
                    .wrapMode = CLAY_TEXT_WRAP_NONE,
                }));
        }

        if(selected) {
            CLAY_AUTO_ID({
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

            const Image* left_border = &menu_border_left;
            const Image* right_border = &menu_border_right;

            CLAY_AUTO_ID({
                .layout = {.sizing = {.height = CLAY_SIZING_FIXED(left_border->height), .width = CLAY_SIZING_FIXED(left_border->width)}},
                .floating =
                    {
                        .attachPoints = {.element = CLAY_ATTACH_POINT_RIGHT_TOP, .parent = CLAY_ATTACH_POINT_LEFT_TOP},
                        .attachTo = CLAY_ATTACH_TO_PARENT,
                    },
                .image = {.imageData = (void*)left_border},
            }){};
            CLAY_AUTO_ID({
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

static void menu_draw_item_list(MenuViewModel* model) {
    CLAY(
        CLAY_ID("MenuItems"),
        {
            .layout =
                {
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                    .childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_TOP},
                },
            .border = {.color = COLOR_GRAY, .width = {.betweenChildren = 1}},
            .clip = {.vertical = true, .childOffset = Clay_GetScrollOffset()},
        }) {
        size_t line_index = 0;
        MenuItemArray_it_t it;
        for(MenuItemArray_it(it, model->items); !MenuItemArray_end_p(it); MenuItemArray_next(it)) {
            MenuItem* item = MenuItemArray_ref(it);
            bool selected = (line_index == model->position);
            menu_draw_item(item, line_index, selected);

            line_index++;
        }
    }
}

static void menu_draw_scrollbar(MenuViewModel* model) {
    CLAY(
        CLAY_ID("Scrollbar"),
        {
            .backgroundColor = COLOR_WHITE,
            .layout =
                {
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .sizing = {.width = CLAY_SIZING_FIXED(model->show_scrollbar ? 3 : 0), .height = CLAY_SIZING_GROW(0)},
                },
            .image = {.imageData = (void*)&scrollbar_background},
        }) {
        if(model->show_scrollbar) {
            CLAY_AUTO_ID({
                .backgroundColor = COLOR_BLACK,
                .layout = {.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_PERCENT(model->scrollbar_len)}},
                .floating =
                    {
                        .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_TOP, .parent = CLAY_ATTACH_POINT_CENTER_TOP},
                        .attachTo = CLAY_ATTACH_TO_PARENT,
                        .offset = {.y = model->scrollbar_offset + 1},
                    },
            }){};
        }
    }
}

static bool menu_layout_callback(void* _model) {
    MenuViewModel* model = _model;
    furi_assert(model);

    CLAY_AUTO_ID({
        .backgroundColor = COLOR_WHITE,
        .layout =
            {
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
                .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                .childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_TOP},
                .padding = {.left = 4, .right = 1, .top = 2, .bottom = 2},
                .childGap = 2,
            },
    }) {
        if(model->title) {
            CLAY_TEXT(clay_helper_string_from_chars(model->title), CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = COLOR_GRAY}));
        }
        CLAY_AUTO_ID({
            .backgroundColor = COLOR_WHITE,
            .layout =
                {
                    .layoutDirection = CLAY_LEFT_TO_RIGHT,
                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                    .childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_TOP},
                    .padding = {.left = 4, .right = 0, .top = 0, .bottom = 0},
                    .childGap = 9,
                },
        }) {
            menu_draw_item_list(model);
            menu_draw_scrollbar(model);
        }
    }

    return false;
}

static bool menu_post_layout_callback(void* _model) {
    MenuViewModel* model = _model;
    furi_check(model);

    bool need_redraw = false;

    Clay_ElementId scrollable_container = CLAY_ID("MenuItems");
    Clay_ElementId scroll_target = MENU_ID(model->position);
    Clay_ElementId scrollbar = CLAY_ID("Scrollbar");

    if(clay_helper_scroll_to_child(scrollable_container, scroll_target, 0, 0, 15)) {
        need_redraw = true;
    }

    Clay_ScrollContainerData scroll_data = Clay_GetScrollContainerData(scrollable_container);

    if(scroll_data.found) {
        float scroll_y = scroll_data.scrollPosition->y;
        float container_height = scroll_data.scrollContainerDimensions.height;
        float content_height = scroll_data.contentDimensions.height;

        if(content_height > container_height) {
            Clay_ElementData scrollbar_data = Clay_GetElementData(scrollbar);

            if(model->show_scrollbar == false) need_redraw = true;
            model->show_scrollbar = true;
            model->scrollbar_len = container_height / content_height;
            model->scrollbar_offset = (-scroll_y) / content_height * scrollbar_data.boundingBox.height;
        } else {
            if(model->show_scrollbar == true) need_redraw = true;
            model->show_scrollbar = false;
        }
    }

    return need_redraw;
}

static void menu_process_up_down(Menu* menu, int8_t delta) {
    with_view_model(
        menu->view,
        MenuViewModel * model,
        {
            size_t items_count = MenuItemArray_size(model->items);
            if(items_count > 0) {
                size_t new_position = (model->position + delta + items_count) % items_count;
                model->position = new_position;
            }
        },
        true);
}

static void menu_process_left_right(Menu* menu, int8_t delta) {
    MenuItem* selected_item = NULL;
    size_t new_index = 0;
    MenuItemSelectorCallback callback = NULL;

    with_view_model(
        menu->view,
        MenuViewModel * model,
        {
            selected_item = MenuItemArray_get(model->items, model->position);
            furi_check(selected_item);
            if(selected_item->type == MenuItemSubTypeSelector) {
                size_t index_max = selected_item->selector.max_index;
                new_index = selected_item->selector.value_index;
                if(new_index < -delta && delta < 0) {
                    new_index = 0;
                } else if(new_index + delta >= index_max && delta > 0) {
                    new_index = index_max - 1;
                } else {
                    new_index += delta;
                }
                callback = selected_item->selector.callback;
                furi_check(callback);
            }
        },
        false);
    if(callback) {
        callback(selected_item, new_index, menu->context);
    }
}

static bool menu_input_callback(InputEvent* event, void* context) {
    furi_check(context);
    Menu* menu = context;
    bool consumed = false;

    if(event->type == InputTypePress && event->key == InputKeyOk) {
        size_t selected_id = 0;
        MenuItem* selected_item = NULL;
        with_view_model(
            menu->view,
            MenuViewModel * model,
            {
                selected_item = MenuItemArray_get(model->items, model->position);
                furi_check(selected_item);
                selected_id = selected_item->id;
            },
            false);
        if(menu->callback) {
            menu->callback(selected_item, selected_id, menu->context);
        }
        consumed = true;
    } else if(event->type == InputTypePress && event->key == InputKeyBack) {
        if(menu->callback) {
            menu->callback(NULL, 0, menu->context);
        }
        consumed = true;
    } else if(event->type == InputTypePress || event->type == InputTypeRepeat) {
        if(event->key == InputKeyUp || event->key == InputKeyDown) {
            menu_process_up_down(menu, event->key == InputKeyUp ? -1 : 1);
            consumed = true;
        } else if(event->key == InputKeyLeft || event->key == InputKeyRight) {
            menu_process_left_right(menu, event->key == InputKeyLeft ? -1 : 1);
            consumed = true;
        }
    }

    return consumed;
}

Menu* menu_alloc(View* view) {
    furi_check(view);
    Menu* menu = malloc(sizeof(Menu));
    menu->view = view;

    view_allocate_model(menu->view, ViewModelTypeLockFree, sizeof(MenuViewModel));
    with_view_model(menu->view, MenuViewModel * model, { MenuItemArray_init(model->items); }, false);

    view_set_layout_callback(menu->view, menu_layout_callback);
    view_set_post_layout_callback(menu->view, menu_post_layout_callback);
    view_set_input_callback(menu->view, menu_input_callback, menu);

    return menu;
}

void menu_free(Menu* menu) {
    furi_check(menu);
    menu->callback = NULL;
    with_view_model(
        menu->view,
        MenuViewModel * model,
        {
            if(model->title) free(model->title);
            MenuItemArray_it_t it;
            for(MenuItemArray_it(it, model->items); !MenuItemArray_end_p(it); MenuItemArray_next(it)) {
                MenuItem* item = MenuItemArray_ref(it);

                if(item->label) free(item->label);

                if(item->type == MenuItemSubTypeLabel) {
                    if(item->sub_label) free(item->sub_label);
                } else if(item->type == MenuItemSubTypeSelector) {
                    furi_string_free(item->selector.value_text);
                }
            }
            MenuItemArray_clear(model->items);
        },
        false);
    view_set_layout_callback(menu->view, NULL);
    view_set_post_layout_callback(menu->view, NULL);
    view_set_input_callback(menu->view, NULL, NULL);
    view_free_model(menu->view);
    free(menu);
}

void menu_set_title(Menu* menu, const char* title) {
    with_view_model(
        menu->view,
        MenuViewModel * model,
        {
            if(model->title) free(model->title);
            model->title = title ? strdup(title) : NULL;
        },
        true);
}

void menu_set_position(Menu* menu, size_t item_id) {
    furi_check(menu);
    with_view_model(
        menu->view,
        MenuViewModel * model,
        {
            size_t position = 0;
            MenuItemArray_it_t it;
            for(MenuItemArray_it(it, model->items); !MenuItemArray_end_p(it); MenuItemArray_next(it)) {
                MenuItem* item = MenuItemArray_ref(it);
                if(item->id == item_id) {
                    model->position = position;
                    break;
                }
                position++;
            }
        },
        true);
}

void menu_set_callback(Menu* menu, MenuItemCallback callback, void* context) {
    furi_check(menu);
    menu->callback = callback;
    menu->context = context;
}

MenuItem* menu_add_item(Menu* menu, const char* label, size_t id, MenuItemSubType type) {
    furi_check(menu);
    furi_check(label);
    MenuItem* item = NULL;

    with_view_model(
        menu->view,
        MenuViewModel * model,
        {
            item = MenuItemArray_push_new(model->items);

            item->label = strdup(label);
            item->id = id;
            item->type = type;
            if(type == MenuItemSubTypeSelector) {
                item->selector.value_text = furi_string_alloc();
            }
        },
        true);
    return item;
}

void menu_item_sublabel_set(Menu* menu, MenuItem* item, const char* text) {
    furi_check(menu);
    furi_check(item);
    furi_check(item->type == MenuItemSubTypeLabel);
    with_view_model(
        menu->view,
        MenuViewModel * model,
        {
            if(item->sub_label) free(item->sub_label);
            item->sub_label = text ? strdup(text) : NULL;
        },
        true);
}

void menu_item_selector_configure(Menu* menu, MenuItem* item, size_t max_count, MenuItemSelectorCallback callback) {
    furi_check(menu);
    furi_check(item);
    furi_check(item->type == MenuItemSubTypeSelector);
    furi_check(callback);
    with_view_model(
        menu->view,
        MenuViewModel * model,
        {
            item->selector.max_index = max_count;
            item->selector.callback = callback;
        },
        false);
}

void menu_item_selector_set_value(Menu* menu, MenuItem* item, size_t index, const char* text) {
    furi_check(menu);
    furi_check(item);
    furi_check(item->type == MenuItemSubTypeSelector);
    with_view_model(
        menu->view,
        MenuViewModel * model,
        {
            item->selector.value_index = index;
            bool is_first = index == 0;
            bool is_last = index == item->selector.max_index - 1;
            furi_string_printf(item->selector.value_text, "%s%s%s", is_first ? " " : "< ", text, is_last ? " " : " >");
        },
        true);
}

size_t menu_item_selector_get_value(Menu* menu, MenuItem* item) {
    furi_check(menu);
    furi_check(item);
    furi_check(item->type == MenuItemSubTypeSelector);
    size_t index = 0;
    with_view_model(menu->view, MenuViewModel * model, { index = item->selector.value_index; }, false);
    return index;
}
