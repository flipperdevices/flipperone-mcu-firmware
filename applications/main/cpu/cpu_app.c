#include <furi.h>
#include <furi_bsp.h>
#include <gui/gui.h>
#include <gui/clay_helper.h>

#define TAG "CpuApp"

#define cpu_app_MENU_ID(x) CLAY_SIDI(CLAY_STRING("CpuAppMenu"), x)

#define cpu_app_MESSAGE_QUEUE_SIZE 64

typedef enum {
    CpuAppMenuItemStart,
    CpuAppMenuItemStop,
    CpuAppMenuItemReset,
    CpuAppMenuItemClose,
} CpuAppMenuItem;

static const char* cpu_app_menu_items[] = {
    [CpuAppMenuItemStart] = "Start",
    [CpuAppMenuItemStop] = "Stop",
    [CpuAppMenuItemReset] = "Reset",
    [CpuAppMenuItemClose] = "Close",
};

static size_t cpu_app_menu_items_count = COUNT_OF(cpu_app_menu_items);

typedef struct {
    size_t selected_index;
} CpuAppModel;

typedef enum {
    CpuAppMessageTypeStart,
    CpuAppMessageTypeStop,
    CpuAppMessageTypeReset,
    CpuAppMessageTypeClose,
} CpuAppMessageType;

typedef struct {
    CpuAppMessageType type;
} CpuAppMessage;

typedef struct {
    Gui* gui;
    View* view;
    FuriEventLoop* event_loop;
    FuriMessageQueue* app_queue;
} CpuApp;

static bool cpu_app_layout(void* _model) {
    furi_assert(_model);
    CpuAppModel* model = (CpuAppModel*)_model;
    Clay_Sizing layoutExpand = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
    Clay_BorderElementConfig contentBorders = {.color = COLOR_BLACK, .width = {.top = 1, .left = 1, .right = 1, .bottom = 1}};

    CLAY(
        CLAY_APP_ID("OuterContainer"),
        {.backgroundColor = COLOR_WHITE,
         .layout = {
             .layoutDirection = CLAY_TOP_TO_BOTTOM,
             .sizing = layoutExpand,
             .padding = {4, 4, 4, 3},
             .childGap = 4,
         }}) {
        CLAY(
            CLAY_APP_ID("Header"),
            {
                .layout =
                    {
                        .sizing = {.height = CLAY_SIZING_FIXED(14), .width = CLAY_SIZING_GROW(0)},
                        .childGap = 8,
                        .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                    },
            }) {
            CLAY_AUTO_ID({.layout = {.padding = {8, 8, 4, 4}, .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER}}}) {
                CLAY_TEXT(CLAY_STRING("CPU"), CLAY_TEXT_CONFIG({.fontId = FontButton, .textColor = COLOR_BLACK}));
            }
        }
        CLAY(
            CLAY_APP_ID("MainContent"),
            {
                .clip = {.vertical = true},
                .layout =
                    {
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        .childGap = 8,
                        .padding = {6, 6, 6, 6},
                        .sizing = layoutExpand,
                        .childAlignment = {.y = CLAY_ALIGN_Y_CENTER, .x = CLAY_ALIGN_X_CENTER},
                    },
            }) {
            for(uint32_t i = 0; i < cpu_app_menu_items_count; i++) {
                bool selected = (i == model->selected_index);
                CLAY(
                    cpu_app_MENU_ID(i),
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
                        clay_helper_string_from_chars(cpu_app_menu_items[i]),
                        CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = selected ? COLOR_WHITE : COLOR_BLACK}));
                }
            }
        }
    }

    return false;
}

static void cpu_app_send_message(CpuApp* instance, CpuAppMessageType type) {
    CpuAppMessage message = {.type = type};
    furi_check(furi_message_queue_put(instance->app_queue, &message, 2) == FuriStatusOk);
}

static void cpu_app_input_menu(CpuApp* instance, size_t selected_index) {
    switch(selected_index) {
    case CpuAppMenuItemStart: {
        cpu_app_send_message(instance, CpuAppMessageTypeStart);
    } break;
    case CpuAppMenuItemStop: {
        cpu_app_send_message(instance, CpuAppMessageTypeStop);
    } break;
    case CpuAppMenuItemReset:
        cpu_app_send_message(instance, CpuAppMessageTypeReset);
        break;
    case CpuAppMenuItemClose:
        cpu_app_send_message(instance, CpuAppMessageTypeClose);
        break;
    }
}

static bool cpu_app_model_menu_next(CpuAppModel* model, void* context) {
    model->selected_index = (model->selected_index + 1) % cpu_app_menu_items_count;
    return true;
}

static bool cpu_app_model_menu_prev(CpuAppModel* model, void* context) {
    model->selected_index = (model->selected_index - 1 + cpu_app_menu_items_count) % cpu_app_menu_items_count;
    return true;
}

static bool cpu_app_input_menu_get_selected_index(CpuAppModel* model, void* context) {
    furi_check(context);
    size_t* selected_index = context;
    *selected_index = model->selected_index;
    return false;
}

static void cpu_app_model_apply(CpuApp* instance, bool (*callback)(CpuAppModel* model, void* context), void* context) {
    bool update;
    with_view_model(instance->view, CpuAppModel * model, { update = callback(model, context); }, update);
}

static bool cpu_app_input(InputEvent* event, void* context) {
    furi_check(context);
    CpuApp* instance = context;
    bool consumed = false;

    if(event->type == InputTypeLong) {
        if(event->key == InputKeyBack) {
            consumed = true;
        }
    }

    if(event->type == InputTypePress) {
        if(event->key == InputKeyUp) {
            cpu_app_model_apply(instance, cpu_app_model_menu_prev, NULL);
        } else if(event->key == InputKeyDown) {
            cpu_app_model_apply(instance, cpu_app_model_menu_next, NULL);
        } else if(event->key == InputKeyOk) {
            size_t selected_index;
            cpu_app_model_apply(instance, cpu_app_input_menu_get_selected_index, &selected_index);
            cpu_app_input_menu(instance, selected_index);
        }
    }
    return consumed;
}

static void furi_hal_bsp_linux_reset(void) {
    furi_bsp_main_reset();
}

static void furi_hal_bsp_linux_start(void) {
    uint32_t status = furi_bsp_expander_main_read_output();
    FURI_LOG_I(TAG, "Current expander output status: 0x%02lX", status);
    status |= OutputExpMainUsb20Sel | OutputExpMainVcc5v0SysS5En;
    FURI_LOG_I(TAG, "Setting expander output status: 0x%02lX", status);
    furi_bsp_expander_main_write_output(status);
}

static void furi_hal_bsp_linux_stop(void) {
    uint32_t status = furi_bsp_expander_main_read_output();
    FURI_LOG_I(TAG, "Current expander output status: 0x%02lX", status);
    status &= ~(OutputExpMainUsb20Sel | OutputExpMainVcc5v0SysS5En);
    FURI_LOG_I(TAG, "Setting expander output status: 0x%02lX", status);
    furi_bsp_expander_main_write_output(status);
}

static void cpu_app_message_logic(FuriEventLoopObject* object, void* context) {
    furi_check(context);
    CpuApp* instance = context;
    furi_check(object == instance->app_queue);

    CpuAppMessage message;
    while(furi_message_queue_get(instance->app_queue, &message, 0) == FuriStatusOk) {
        switch(message.type) {
        case CpuAppMessageTypeStart:
        case CpuAppMessageTypeReset:
            furi_hal_bsp_linux_reset();
            furi_bsp_expander_main_set_control(FuriBspControlExpanderMainCpu);
            furi_hal_bsp_linux_start();
            break;
        case CpuAppMessageTypeStop:
            furi_hal_bsp_linux_reset();
            furi_hal_bsp_linux_stop();
            break;
        case CpuAppMessageTypeClose:
            furi_hal_bsp_linux_reset();
            furi_thread_signal(furi_thread_get_current(), FuriSignalExit, NULL);
            break;
        default:
            furi_assert(false);
            break;
        }
    }
}

static CpuApp* cpu_app_alloc(void) {
    CpuApp* instance = malloc(sizeof(CpuApp));
    instance->gui = furi_record_open(RECORD_GUI);
    instance->event_loop = furi_event_loop_alloc();
    instance->app_queue = furi_message_queue_alloc(cpu_app_MESSAGE_QUEUE_SIZE, sizeof(CpuAppMessage));

    furi_event_loop_subscribe_message_queue(instance->event_loop, instance->app_queue, FuriEventLoopEventIn, cpu_app_message_logic, instance);

    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLockFree, sizeof(CpuAppModel));
    view_set_layout_callback(instance->view, cpu_app_layout);
    view_set_input_callback(instance->view, cpu_app_input, instance);
    gui_add_view(instance->gui, instance->view, GuiViewPriorityApplication);
    return instance;
}

static void cpu_app_free(CpuApp* instance) {
    gui_remove_view(instance->gui, instance->view);
    furi_record_close(RECORD_GUI);
    view_free(instance->view);
    furi_event_loop_unsubscribe(instance->event_loop, instance->app_queue);
    furi_event_loop_free(instance->event_loop);
    furi_message_queue_free(instance->app_queue);
    free(instance);
}

int32_t cpu_app(void* p) {
    UNUSED(p);
    CpuApp* instance = cpu_app_alloc();
    furi_event_loop_run(instance->event_loop);
    cpu_app_free(instance);
    return 0;
}
