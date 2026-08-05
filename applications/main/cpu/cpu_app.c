#include "cpu_app.h"
#include <furi.h>
#include <furi_bsp.h>
#include <gui/gui.h>
#include <gui/clay_helper.h>
#include <gui/modules/popup_menu.h>
#include <drivers/display/display_jd9853_reg.h>
#include <drivers/pio_get_frame/pio_get_frame.h>
#include <assets.h>
#include <pd/pd.h>
#include <power/power.h>
#include <i2c_negotiator/i2c_negotiator.h>

#define TAG "CpuApp"

#define CPU_APP_MESSAGE_QUEUE_SIZE 64

#define CPU_ARG_START   "start"
#define CPU_ARG_MASKROM "maskrom"

typedef enum {
    CpuMenuItemStop = 0,
    CpuMenuItemReboot,
} CpuMenuItem;

typedef struct {
    Image frame;
} CpuAppModel;

typedef enum {
    CpuAppMessageTypeStart,
    CpuAppMessageTypeReset,
    CpuAppMessageTypeMaskrom,
    CpuAppMessageTypeShutdown,
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
    View* display_view;
    View* menu_view;
    PopupMenu* menu;
    FuriEventLoop* event_loop;
    FuriMessageQueue* app_queue;
    PioGetFrame* pio_get_frame;
    size_t skip_frames;
} CpuApp;

static void furi_hal_reset_pd_and_charger(void) {
    Pd* pd = furi_record_open(RECORD_PD);
    PdDevice pd_device;
    pd_reset_config(pd);
    furi_record_close(RECORD_PD);

    Power* power = furi_record_open(RECORD_POWER);
    PowerDevice power_device;
    power_bq25792_reset_config(power);
    furi_record_close(RECORD_POWER);
}

static void furi_hal_bsp_linux_reset(void) {
    furi_bsp_main_reset();
}

static bool furi_hal_bsp_linux_is_load(void) {
    const uint32_t mask = OutputExpMainUsb20Sel | OutputExpMainVcc5v0SysS5En;
    uint32_t status = furi_bsp_expander_main_read_output();
    return (status & mask) == mask;
}

static void furi_hal_bsp_linux_start(void) {
    uint32_t status = furi_bsp_expander_main_read_output();
    FURI_LOG_I(TAG, "Current expander output status: 0x%02lX", status);
    status |= OutputExpMainUsb20Sel | OutputExpMainVcc5v0SysS5En;
    FURI_LOG_I(TAG, "Setting expander output status: 0x%02lX", status);
    furi_bsp_expander_main_write_output(status);
}

static void furi_hal_bsp_linux_maskrom(void) {
    uint32_t status = furi_bsp_expander_main_read_output();
    FURI_LOG_I(TAG, "Current expander output status: 0x%02lX", status);
    status |= OutputExpMainUsb20Sel | OutputExpMainVcc5v0SysS5En | OutputExpMainExpander17;
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
    }

    return false;
}

static void cpu_app_send_message(CpuApp* instance, CpuAppMessageType type) {
    CpuAppMessage message = {.type = type};
    furi_check(furi_message_queue_put(instance->app_queue, &message, 2) == FuriStatusOk);
}

static bool cpu_app_model_init(CpuAppModel* model, void* context) {
    model->frame = display_starting;
    return true;
}

static bool cpu_app_model_new_frame(CpuAppModel* model, void* context) {
    model->frame.data = context;
    model->frame.width = JD9853_WIDTH;
    model->frame.height = JD9853_HEIGHT;
    return true;
}

static void cpu_app_model_apply(CpuApp* instance, bool (*callback)(CpuAppModel* model, void* context), void* context) {
    bool update;
    with_view_model(instance->display_view, CpuAppModel * model, { update = callback(model, context); }, update);
}

static void __isr __not_in_flash_func(cpu_app_pio_get_frame_isr)(uint8_t* data, size_t size, void* context) {
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
            if(!furi_hal_bsp_linux_is_load()) {
                furi_hal_bsp_linux_reset();
                furi_hal_bsp_linux_start();
            }
            furi_bsp_expander_main_set_control(FuriBspControlExpanderMainCpu);
            break;
        case CpuAppMessageTypeReset:
            furi_hal_reset_pd_and_charger();
            furi_hal_bsp_linux_reset();
            furi_bsp_expander_main_set_control(FuriBspControlExpanderMainCpu);
            furi_hal_bsp_linux_start();
            instance->skip_frames = 2;
            cpu_app_model_apply(instance, cpu_app_model_init, NULL);
            break;
        case CpuAppMessageTypeShutdown:
            furi_hal_reset_pd_and_charger();
            furi_hal_bsp_linux_reset();
            furi_thread_signal(furi_thread_get_current(), FuriSignalExit, NULL);
            break;
        case CpuAppMessageTypeMaskrom:
            furi_hal_reset_pd_and_charger();
            furi_hal_bsp_linux_reset();
            furi_bsp_expander_main_set_control(FuriBspControlExpanderMainCpu);
            furi_hal_bsp_linux_maskrom();
            break;
        case CpuAppMessageTypeNewFrame:
            if(instance->skip_frames > 0) {
                instance->skip_frames--;
            } else {
                cpu_app_model_apply(instance, cpu_app_model_new_frame, message.as.new_frame.data);
            }
            break;
        default:
            furi_assert(false);
            break;
        }
    }
}

// We will bring this back when we figure out how to catch shutdown event from the CPU
// void cpu_app_menu_shutdown_click_callback(bool pressed, void* context) {
//     furi_check(context);
//     CpuApp* instance = context;
//     i2c_negotiator_input_sw_button_event(SwPowerKey, pressed, NULL);
// }

static void cpu_app_menu_item_selected(size_t item_id, void* context) {
    furi_check(context);
    CpuApp* instance = context;

    if(item_id == CpuMenuItemStop) {
        cpu_app_send_message(instance, CpuAppMessageTypeShutdown);
    } else if(item_id == CpuMenuItemReboot) {
        popup_menu_set_visible(instance->menu, false);
        cpu_app_send_message(instance, CpuAppMessageTypeReset);
    }
}

static CpuApp* cpu_app_alloc(void) {
    CpuApp* instance = malloc(sizeof(CpuApp));
    instance->gui = furi_record_open(RECORD_GUI);
    instance->event_loop = furi_event_loop_alloc();
    instance->app_queue = furi_message_queue_alloc(CPU_APP_MESSAGE_QUEUE_SIZE, sizeof(CpuAppMessage));

    instance->pio_get_frame = pio_get_frame_init();
    pio_get_frame_set_callback_rx(instance->pio_get_frame, cpu_app_pio_get_frame_isr, instance);

    furi_event_loop_subscribe_message_queue(instance->event_loop, instance->app_queue, FuriEventLoopEventIn, cpu_app_message_logic, instance);

    instance->display_view = view_alloc();
    view_allocate_model(instance->display_view, ViewModelTypeLockFree, sizeof(CpuAppModel));
    cpu_app_model_apply(instance, cpu_app_model_init, NULL);

    view_set_layout_callback(instance->display_view, cpu_app_layout);
    gui_add_view(instance->gui, instance->display_view, GuiViewPriorityApplication);

    instance->menu_view = view_alloc();
    instance->menu = popup_menu_alloc(instance->menu_view);
    popup_menu_set_callback(instance->menu, cpu_app_menu_item_selected, instance);
    popup_menu_set_title(instance->menu, "Flipper OS");
    popup_menu_add_item(instance->menu, "Stop", CpuMenuItemStop);
    popup_menu_add_item(instance->menu, "Reboot", CpuMenuItemReboot);

    gui_add_view(instance->gui, instance->menu_view, GuiViewPriorityPowerMenu);

    return instance;
}

static void cpu_app_free(CpuApp* instance) {
    gui_remove_view(instance->gui, instance->display_view);
    gui_remove_view(instance->gui, instance->menu_view);
    popup_menu_free(instance->menu);

    furi_record_close(RECORD_GUI);

    view_free(instance->display_view);
    view_free(instance->menu_view);

    furi_event_loop_unsubscribe(instance->event_loop, instance->app_queue);
    furi_event_loop_free(instance->event_loop);
    furi_message_queue_free(instance->app_queue);
    pio_get_frame_deinit(instance->pio_get_frame);
    free(instance);
}

int32_t cpu_app(void* p) {
    CpuApp* instance = cpu_app_alloc();

    if(p) {
        char* arg = (char*)p;
        FURI_LOG_I(TAG, "CPU app started with arg: %s", arg);
        if(strcmp(arg, CPU_ARG_START) == 0) {
            cpu_app_send_message(instance, CpuAppMessageTypeStart);
        } else if(strcmp(arg, CPU_ARG_MASKROM) == 0) {
            cpu_app_send_message(instance, CpuAppMessageTypeMaskrom);
        } else {
            FURI_LOG_E(TAG, "Unknown argument: %s", arg);
        }
    }

    furi_event_loop_run(instance->event_loop);
    cpu_app_free(instance);
    return 0;
}
