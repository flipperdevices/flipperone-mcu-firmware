#include <furi.h>
#include <furi_bsp.h>
#include <gui/gui.h>
#include <gui/clay_helper.h>
#include <drivers/display/display_jd9853_reg.h>
#include <drivers/spi_get_frame/spi_get_frame.h>
#include <assets.h>
#include <pd/pd.h>
#include <power/power.h>
#include <power_menu/power_menu.h>

#define TAG "CpuApp"

#define CPU_APP_MENU_ID(x) CLAY_SIDI(CLAY_STRING("CpuAppMenu"), x)

#define CPU_APP_MESSAGE_QUEUE_SIZE 64

#define CPU_APP_MENU_START   "CPU Start"
#define CPU_APP_MENU_REBOOT  "CPU Reboot"
#define CPU_APP_MENU_MASKROM "CPU Maskrom"
#define CPU_APP_MENU_CLOSE   "CPU Shutdown"

typedef struct {
    Image frame;
} CpuAppModel;

typedef enum {
    CpuAppMessageTypeStart,
    CpuAppMessageTypeReset,
    CpuAppMessageTypeMaskrom,
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
    model->frame = flipper_one_256x144_test_screen_v002;
    return false;
}

static bool cpu_app_model_new_frame(CpuAppModel* model, void* context) {
    model->frame.data = context;
    model->frame.width = JD9853_WIDTH;
    model->frame.height = JD9853_HEIGHT;
    return true;
}

static void cpu_app_model_apply(CpuApp* instance, bool (*callback)(CpuAppModel* model, void* context), void* context) {
    bool update;
    with_view_model(instance->view, CpuAppModel * model, { update = callback(model, context); }, update);
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
            break;
        case CpuAppMessageTypeClose:
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
            cpu_app_model_apply(instance, cpu_app_model_new_frame, message.as.new_frame.data);
            break;
        default:
            furi_assert(false);
            break;
        }
    }
}

void cpu_app_menu_close_click_callback(void* context) {
    furi_check(context);
    CpuApp* instance = context;
    cpu_app_send_message(instance, CpuAppMessageTypeClose);
}

void cpu_app_menu_restart_click_callback(void* context) {
    furi_check(context);
    CpuApp* instance = context;
    cpu_app_send_message(instance, CpuAppMessageTypeReset);
}

static CpuApp* cpu_app_alloc(void) {
    CpuApp* instance = malloc(sizeof(CpuApp));
    instance->gui = furi_record_open(RECORD_GUI);
    instance->event_loop = furi_event_loop_alloc();
    instance->app_queue = furi_message_queue_alloc(CPU_APP_MESSAGE_QUEUE_SIZE, sizeof(CpuAppMessage));

    instance->spi_get_frame = spi_get_frame_init();
    spi_get_frame_set_callback_rx(instance->spi_get_frame, cpu_app_spi_get_frame_isr, instance);

    furi_event_loop_subscribe_message_queue(instance->event_loop, instance->app_queue, FuriEventLoopEventIn, cpu_app_message_logic, instance);

    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLockFree, sizeof(CpuAppModel));
    cpu_app_model_apply(instance, cpu_app_model_init, NULL);

    view_set_layout_callback(instance->view, cpu_app_layout);
    gui_add_view(instance->gui, instance->view, GuiViewPriorityApplication);

    //add some test menu items
    power_menu_add_menu_item(CPU_APP_MENU_CLOSE, (FuriCallbackWithContext){.callback = cpu_app_menu_close_click_callback, .context = instance});
    power_menu_add_menu_item(CPU_APP_MENU_REBOOT, (FuriCallbackWithContext){.callback = cpu_app_menu_restart_click_callback, .context = instance});
    return instance;
}

static void cpu_app_free(CpuApp* instance) {
    power_menu_remove_menu_item(CPU_APP_MENU_CLOSE);
    power_menu_remove_menu_item(CPU_APP_MENU_REBOOT);

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
    CpuApp* instance = cpu_app_alloc();

    if(p) {
        char* arg = (char*)p;
        FURI_LOG_I(TAG, "CPU app started with arg: %s", arg);
        if(strcmp(arg, CPU_APP_MENU_START) == 0) {
            cpu_app_send_message(instance, CpuAppMessageTypeStart);
        } else if(strcmp(arg, CPU_APP_MENU_MASKROM) == 0) {
            cpu_app_send_message(instance, CpuAppMessageTypeMaskrom);
        } else {
            FURI_LOG_E(TAG, "Unknown argument: %s", arg);
        }
    }

    furi_event_loop_run(instance->event_loop);
    cpu_app_free(instance);
    return 0;
}
