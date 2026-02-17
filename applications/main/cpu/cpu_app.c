#include <furi.h>
#include <furi_bsp.h>
#include <gui/gui.h>
#include <gui/clay_helper.h>
#include <drivers/display/display_jd9853_reg.h>
#include <drivers/spi_get_frame/spi_get_frame.h>

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
    Image frame;
} CpuAppModel;

typedef enum {
    CpuAppMessageTypeStart,
    CpuAppMessageTypeStop,
    CpuAppMessageTypeReset,
    CpuAppMessageTypeClose,
    CpuAppMessageTypeNewFrame,
} CpuAppMessageType;

typedef struct {
    CpuAppMessageType type;
    union {
        struct {
            uint8_t* data;
            size_t size;
        } new_frame;
    } as;
} CpuAppMessage;

typedef struct {
    Gui* gui;
    View* view;
    FuriEventLoop* event_loop;
    FuriMessageQueue* app_queue;
    SpiGetFrame* spi_get_frame;
} CpuApp;

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

static bool cpu_app_layout(void* _model) {
    furi_assert(_model);
    CpuAppModel* model = (CpuAppModel*)_model;
    Clay_Sizing layout_expand = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
    Clay_Sizing layout_screen = {.width = CLAY_SIZING_FIXED(JD9853_WIDTH), .height = CLAY_SIZING_FIXED(JD9853_HEIGHT)};

    CLAY(
        CLAY_APP_ID("OuterContainer"),
        {.backgroundColor = COLOR_WHITE,
         .layout = {
             .layoutDirection = CLAY_TOP_TO_BOTTOM,
             .sizing = layout_expand,
             .childGap = 4,
         }}) {
        CLAY(
            CLAY_APP_ID("ImageContainer"),
            {
                .backgroundColor = COLOR_BLACK,
                .layout =
                    {
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        .childGap = 8,
                        .sizing = layout_screen,
                        .childAlignment = {.y = CLAY_ALIGN_Y_CENTER, .x = CLAY_ALIGN_X_CENTER},
                    },
            }) {
            if(model->frame.data) {
                Image* image = &model->frame;
                CLAY_AUTO_ID({
                    .layout = {.sizing = layout_screen},
                    .image = {.imageData = image},
                }) {
                }
            }
        }
        CLAY(
            CLAY_APP_ID("MainContent"),
            {
                .backgroundColor = COLOR_WHITE,
                .layout =
                    {
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        .childGap = 8,
                        .padding = {4, 4, 4, 4},
                        .sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0)},
                        .childAlignment = {.y = CLAY_ALIGN_Y_CENTER, .x = CLAY_ALIGN_X_CENTER},
                    },
                .floating =
                    {
                        .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER, .parent = CLAY_ATTACH_POINT_CENTER_CENTER},
                        .attachTo = CLAY_ATTACH_TO_PARENT,
                    },
                .border = {.color = COLOR_BLACK, .width = {.top = 1, .left = 1, .right = 1, .bottom = 1}},
                .cornerRadius = CLAY_CORNER_RADIUS(4),
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

static bool cpu_app_model_init(CpuAppModel* model, void* context) {
    model->frame = (Image){
        .format = ImageFormatRawGray8,
        .width = JD9853_WIDTH,
        .height = JD9853_HEIGHT,
        .data = NULL,
    };

    return false;
}

static bool cpu_app_model_new_frame(CpuAppModel* model, void* context) {
    model->frame.data = context;
    return true;
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

static void __isr __not_in_flash_func(cpu_app_spi_get_frame_isr)(uint8_t* data, size_t size, void* context) {
    CpuApp* instance = context;

    CpuAppMessage message = {
        .type = CpuAppMessageTypeNewFrame,
        .as.new_frame =
            {
                .data = data,
                .size = size,
            },
    };

    furi_check(furi_message_queue_put(instance->app_queue, &message, 0) == FuriStatusOk);
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
        case CpuAppMessageTypeNewFrame:
            FURI_LOG_I(TAG, "Received new frame data: %p (size: %zu)", message.as.new_frame.data, message.as.new_frame.size);
            cpu_app_model_apply(instance, cpu_app_model_new_frame, message.as.new_frame.data);
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

    instance->spi_get_frame = spi_get_frame_init();
    spi_get_frame_set_callback_rx(instance->spi_get_frame, cpu_app_spi_get_frame_isr, instance);

    furi_event_loop_subscribe_message_queue(instance->event_loop, instance->app_queue, FuriEventLoopEventIn, cpu_app_message_logic, instance);

    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLockFree, sizeof(CpuAppModel));
    cpu_app_model_apply(instance, cpu_app_model_init, NULL);

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
    spi_get_frame_deinit(instance->spi_get_frame);
    free(instance);
}

int32_t cpu_app(void* p) {
    UNUSED(p);
    CpuApp* instance = cpu_app_alloc();
    furi_event_loop_run(instance->event_loop);
    cpu_app_free(instance);
    return 0;
}
