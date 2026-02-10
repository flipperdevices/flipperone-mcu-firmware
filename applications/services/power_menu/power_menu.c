#include <furi.h>
#include <gui/gui.h>
#include <gui/clay_helper.h>

#define TAG              "PowerMenu"
#define POWER_MENU_ID(x) CLAY_SIDI(CLAY_STRING("Menu"), x)

static const char* power_menu_items[] = {
    "Power Off",
    "Restart",
    "Sleep",
    "Cancel",
};
static size_t power_menu_items_count = COUNT_OF(power_menu_items);

typedef struct {
    bool visible;
    size_t selected_index;
} PowerMenuModel;

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriEventLoop* event_loop;
} PowerMenu;

static bool power_menu_layout(void* _model) {
    furi_assert(_model);
    PowerMenuModel* model = (PowerMenuModel*)_model;

    if(!model->visible) {
        return false;
    }

    CLAY(
        CLAY_APP_ID("Container"),
        {
            .backgroundColor = COLOR_WHITE,
            .layout =
                {
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0)},
                    .childGap = 4,
                    .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                    .padding = {4, 4, 4, 4},
                },
            .clip = {.vertical = true, .childOffset = Clay_GetScrollOffset()},
            .floating =
                {
                    .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER, .parent = CLAY_ATTACH_POINT_CENTER_CENTER},
                    .attachTo = CLAY_ATTACH_TO_ROOT,
                },
            .border = {.color = COLOR_BLACK, .width = {.top = 1, .left = 1, .right = 1, .bottom = 1}},
            .cornerRadius = CLAY_CORNER_RADIUS(4),
        }) {
        for(uint32_t i = 0; i < power_menu_items_count; i++) {
            bool selected = (i == model->selected_index);
            CLAY(
                POWER_MENU_ID(i),
                {
                    .layout =
                        {
                            .sizing = {.width = CLAY_SIZING_FIXED(80), .height = CLAY_SIZING_FIXED(13)},
                            .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                        },
                    .backgroundColor = selected ? COLOR_BLACK : COLOR_WHITE,
                    .cornerRadius = CLAY_CORNER_RADIUS(2),
                }) {
                CLAY_TEXT(
                    clay_helper_string_from_chars(power_menu_items[i]),
                    CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = selected ? COLOR_WHITE : COLOR_BLACK}));
            }
        }
    }

    return false;
}

static bool power_menu_input(InputEvent* event, void* context) {
    furi_check(context);
    PowerMenu* instance = context;
    bool consumed = false;
    bool visible;
    with_view_model(instance->view_port, PowerMenuModel * model, { visible = model->visible; }, false);

    if(event->key == InputKey3) {
        if(event->type == InputTypeLong) {
            with_view_model(
                instance->view_port,
                PowerMenuModel * model,
                {
                    model->visible = !model->visible;
                    FURI_LOG_I(TAG, "Visibility toggled to %d", model->visible);
                },
                true);
            consumed = true;
        }
    }

    if(visible) {
        if(event->type == InputTypePress) {
            if(event->key == InputKeyUp) {
                with_view_model(
                    instance->view_port,
                    PowerMenuModel * model,
                    { model->selected_index = (model->selected_index - 1 + power_menu_items_count) % power_menu_items_count; },
                    true);
                consumed = true;
            } else if(event->key == InputKeyDown) {
                with_view_model(
                    instance->view_port, PowerMenuModel * model, { model->selected_index = (model->selected_index + 1) % power_menu_items_count; }, true);
                consumed = true;
            } else if(event->key == InputKeyOk) {
                with_view_model(instance->view_port, PowerMenuModel * model, { model->visible = false; }, true);
                consumed = true;
            } else if(event->key == InputKeyBack) {
                with_view_model(instance->view_port, PowerMenuModel * model, { model->visible = false; }, true);
                consumed = true;
            }
        }
    }

    return consumed;
}

static PowerMenu* power_menu_alloc(void) {
    PowerMenu* instance = malloc(sizeof(PowerMenu));
    instance->gui = furi_record_open(RECORD_GUI);
    instance->event_loop = furi_event_loop_alloc();

    instance->view_port = view_port_alloc();
    view_port_set_transparent(instance->view_port, true);
    view_port_allocate_model(instance->view_port, ViewPortModelTypeLockFree, sizeof(PowerMenuModel));
    view_port_set_layout_callback(instance->view_port, power_menu_layout);
    view_port_set_input_callback(instance->view_port, power_menu_input, instance);
    gui_add_view_port(instance->gui, instance->view_port, GuiViewPriorityMenu);
    return instance;
}

static void power_menu_free(PowerMenu* instance) {
    gui_remove_view_port(instance->gui, instance->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(instance->view_port);
    furi_event_loop_free(instance->event_loop);
    free(instance);
}

int32_t power_menu(void* p) {
    PowerMenu* instance = power_menu_alloc();
    furi_event_loop_run(instance->event_loop);
    power_menu_free(instance);
    return 0;
}
