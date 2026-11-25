#include "cpu_app.h"
#include <furi.h>
#include <furi_bsp.h>
#include <furi_hal_resources.h>
#include <gui/gui.h>
#include <gui/clay_helper.h>
#include <gui/modules/popup_menu.h>
#include <drivers/display/display_jd9853_reg.h>
#include <drivers/pio_get_frame/pio_get_frame.h>
#include <assets.h>
#include <host/host.h>
#include <i2c_negotiator/i2c_negotiator.h>

#define TAG "CpuApp"

#define CPU_APP_MESSAGE_QUEUE_SIZE 64
#define CPU_APP_HOST_STATE_FLAG    (1 << 0)

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
    Host* host;
    FuriPubSubSubscription* host_subscription;
    FuriEventFlag* host_state_flag;
} CpuApp;

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
                CLAY(
                    CLAY_APP_ID("FrameImage"),
                    {
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

// Swap one placeholder for another, never a frame from the host.
static bool cpu_app_model_placeholder(CpuAppModel* model, void* context) {
    const Image* image = context;
    bool placeholder = model->frame.data == display_starting.data || model->frame.data == display_waiting.data ||
                       model->frame.data == display_not_responding.data;
    if(!placeholder || model->frame.data == image->data) return false;
    model->frame = *image;
    return true;
}

/* Update the model to the latest frame WITHOUT triggering a redraw. The GUI
 * service decides how to present the frame (direct blit or Clay composite);
 * keeping the model in sync ensures a Clay redraw always uses the latest
 * frame, e.g. when the menu is opened over it. */
static void cpu_app_model_set_frame(CpuApp* instance, uint8_t* data) {
    with_view_model(
        instance->display_view,
        CpuAppModel * model,
        {
            // A placeholder may have been 1 bit or masked
            model->frame.format = ImageFormatRawGray8;
            model->frame.mask = NULL;
            model->frame.data = data;
            model->frame.width = JD9853_WIDTH;
            model->frame.height = JD9853_HEIGHT;
        },
        false);
}

static void cpu_app_model_apply(CpuApp* instance, bool (*callback)(CpuAppModel* model, void* context), void* context) {
    bool update;
    with_view_model(instance->display_view, CpuAppModel * model, { update = callback(model, context); }, update);
}

// Until the host sends a frame, show what state it is in.
static void cpu_app_show_host_state(CpuApp* instance) {
    const Image* image = NULL;
    switch(host_get_state(instance->host)) {
    case HostStateStarting:
        image = &display_starting;
        break;
    case HostStateRunning:
        image = &display_waiting;
        break;
    case HostStateNotResponding:
        image = &display_not_responding;
        break;
    default:
        return;
    }
    cpu_app_model_apply(instance, cpu_app_model_placeholder, (void*)image);
}

// Host thread: hand the change over to the app's event loop.
static void cpu_app_host_state_callback(const void* message, void* context) {
    UNUSED(message);
    CpuApp* instance = context;
    furi_event_flag_set(instance->host_state_flag, CPU_APP_HOST_STATE_FLAG);
}

static void cpu_app_host_state_logic(FuriEventLoopObject* object, void* context) {
    furi_check(context);
    CpuApp* instance = context;
    furi_check(object == instance->host_state_flag);
    furi_event_flag_clear(instance->host_state_flag, CPU_APP_HOST_STATE_FLAG);
    cpu_app_show_host_state(instance);
}

static void cpu_app_do_shutdown(CpuApp* instance) {
    host_power_off(instance->host);
    // Do not leave the old frame on screen after the app exits.
    gui_clear_frame(instance->gui);
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

    if(furi_message_queue_put(instance->app_queue, &message, 0) != FuriStatusOk) {
        FURI_LOG_E(TAG, "cpu_app_spi_get_frame_isr message = %ld", furi_message_queue_get_count(instance->app_queue));
    }
}

static void cpu_app_message_logic(FuriEventLoopObject* object, void* context) {
    furi_check(context);
    CpuApp* instance = context;
    furi_check(object == instance->app_queue);

    CpuAppMessage message;
    while(furi_message_queue_get(instance->app_queue, &message, 0) == FuriStatusOk) {
        switch(message.type) {
        case CpuAppMessageTypeStart:
            host_power_on(instance->host);
            cpu_app_show_host_state(instance);
            break;
        case CpuAppMessageTypeReset:
            host_power_restart(instance->host);
            instance->skip_frames = 2;
            // Drop the stale frame so the screen shows the "starting" image
            // right away instead of the old picture.
            gui_clear_frame(instance->gui);
            cpu_app_model_apply(instance, cpu_app_model_init, NULL);
            break;
        case CpuAppMessageTypeShutdown:
            FURI_LOG_I(TAG, "CPU app shutdown requested");
            // The actual hardware cleanup happens in cpu_app_do_shutdown()
            // after the event loop exits, so it covers every exit path.
            furi_thread_signal(furi_thread_get_current(), FuriSignalExit, NULL);
            break;
        case CpuAppMessageTypeMaskrom:
            host_power_maskrom(instance->host);
            break;
        case CpuAppMessageTypeNewFrame:
            if(instance->skip_frames > 0) {
                instance->skip_frames--;
            } else {
                /* Always keep the model in sync (no redraw) and hand the frame
                 * to the GUI: it decides internally whether to blit it directly
                 * to the display (menu hidden) or composite it via Clay (menu
                 * drawn on top). */
                cpu_app_model_set_frame(instance, message.as.new_frame.data);
                gui_push_frame(instance->gui, message.as.new_frame.data);
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
    instance->host = furi_record_open(RECORD_HOST);
    instance->event_loop = furi_event_loop_alloc();
    instance->app_queue = furi_message_queue_alloc(CPU_APP_MESSAGE_QUEUE_SIZE, sizeof(CpuAppMessage));

    instance->pio_get_frame = pio_get_frame_init(&gpio_cpu_spi_cs, &gpio_cpu_spi_sck, &gpio_cpu_spi_mosi);
    pio_get_frame_set_callback_rx(instance->pio_get_frame, cpu_app_pio_get_frame_isr, instance);

    furi_event_loop_subscribe_message_queue(instance->event_loop, instance->app_queue, FuriEventLoopEventIn, cpu_app_message_logic, instance);
    instance->host_state_flag = furi_event_flag_alloc();
    furi_event_loop_subscribe_event_flag(instance->event_loop, instance->host_state_flag, FuriEventLoopEventIn, cpu_app_host_state_logic, instance);
    instance->host_subscription = furi_pubsub_subscribe(host_get_pubsub(instance->host), cpu_app_host_state_callback, instance);

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
    gui_set_menu(instance->gui, instance->menu);

    return instance;
}

static void cpu_app_free(CpuApp* instance) {

    gui_set_menu(instance->gui, NULL);
    gui_remove_view(instance->gui, instance->display_view);
    gui_remove_view(instance->gui, instance->menu_view);
    popup_menu_free(instance->menu);

    furi_record_close(RECORD_GUI);

    view_free(instance->display_view);
    view_free(instance->menu_view);

    furi_pubsub_unsubscribe(host_get_pubsub(instance->host), instance->host_subscription);
    furi_event_loop_unsubscribe(instance->event_loop, instance->host_state_flag);
    furi_event_flag_free(instance->host_state_flag);
    furi_record_close(RECORD_HOST);
    furi_event_loop_unsubscribe(instance->event_loop, instance->app_queue);
    furi_event_loop_free(instance->event_loop);
    furi_message_queue_free(instance->app_queue);

    pio_get_frame_deinit(instance->pio_get_frame);

    gui_clear_frame(instance->gui);
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

    cpu_app_do_shutdown(instance);
    cpu_app_free(instance);
    return 0;
}
