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
#include <main/self_check/self_check_screen.h>

#define TAG "CpuApp"

#define CPU_APP_MESSAGE_QUEUE_SIZE 64

#define CPU_ARG_START   "start"
#define CPU_ARG_MASKROM "maskrom"

typedef enum {
    CpuMenuItemStop = 0,
    CpuMenuItemReboot,
} CpuMenuItem;

/* The model is lock free and Clay keeps the image pointer from layout until
 * render, so an Image the renderer can see is never rewritten: frame is swapped
 * whole, to a placeholder or to host_frame, whose only changing field is data.
 * NULL is the self check screen. */
typedef struct {
    const Image* frame;
    Image host_frame;
    const SelfCheckScreen* self_check;
} CpuAppModel;

typedef enum {
    CpuAppMessageTypeStart,
    CpuAppMessageTypeReset,
    CpuAppMessageTypeMaskrom,
    CpuAppMessageTypeShutdown,
    CpuAppMessageTypeNewFrame,
    CpuAppMessageTypeHostEvent,
} CpuAppMessageType;

typedef struct {
    CpuAppMessageType type;
    union {
        struct {
            uint8_t* data;
            size_t size;
        } new_frame;
        HostEvent host_event;
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
    Host* host;
    FuriPubSubSubscription* host_subscription;
    HostEvent host_event; // the last one handled
    SelfCheckScreen* self_check;
    // The self check screen is shown: until the host draws something, its
    // frames that are entirely black do not replace it
    bool self_check_shown;
    // The host restarts: the old kernel can still send frames after it went
    // down, so frames wait until the host reports being up again
    bool host_restarting;
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
            const Image* frame = model->frame;
            if(frame) {
                CLAY(
                    CLAY_APP_ID("FrameImage"),
                    {
                        .layout = {.sizing = layout_screen},
                        .image = {.imageData = (void*)frame},
                    }) {
                }
            } else {
                self_check_screen_layout(model->self_check);
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
    model->frame = &display_starting;
    model->host_frame.format = ImageFormatRawGray8;
    model->host_frame.width = JD9853_WIDTH;
    model->host_frame.height = JD9853_HEIGHT;
    model->self_check = context;
    return true;
}

static bool cpu_app_model_self_check(CpuAppModel* model, void* context) {
    UNUSED(context);
    if(model->frame == NULL) return false;
    model->frame = NULL;
    return true;
}

// Swap one placeholder for another, never a frame from the host.
static bool cpu_app_model_placeholder(CpuAppModel* model, void* context) {
    const Image* image = context;
    if(model->frame == &model->host_frame || model->frame == image) return false;
    model->frame = image;
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
            model->host_frame.data = data;
            // Not before data: the first host frame has none yet
            __atomic_store_n(&model->frame, &model->host_frame, __ATOMIC_RELEASE);
        },
        false);
}

static void cpu_app_model_apply(CpuApp* instance, bool (*callback)(CpuAppModel* model, void* context), void* context) {
    bool update;
    with_view_model(instance->display_view, CpuAppModel * model, { update = callback(model, context); }, update);
}

/* The self check screen, over whatever is on screen, until the host draws
 * something. The host's own frame is dropped first, or the GUI would blit it. */
static void cpu_app_show_self_check(CpuApp* instance) {
    instance->self_check_shown = true;
    gui_clear_frame(instance->gui);
    cpu_app_model_apply(instance, cpu_app_model_self_check, NULL);
}

// Until the host sends a frame, show what state it is in.
static void cpu_app_show_host_state(CpuApp* instance) {
    const Image* image = NULL;
    switch(host_get_state(instance->host)) {
    case HostStateRunning:
        image = &display_waiting;
        break;
    case HostStateNotResponding:
        image = &display_not_responding;
        break;
    default:
        return;
    }
    instance->self_check_shown = false;
    cpu_app_model_apply(instance, cpu_app_model_placeholder, (void*)image);
}

/* The self check screen goes up when the MCU powers the host and when the host
 * restarts by itself; not for a host found running after an MCU reset. */
static void cpu_app_host_event(CpuApp* instance, const HostEvent* event) {
    const HostEvent last = instance->host_event;
    instance->host_event = *event;
    // Up again: its frames count from now on
    if(event->cpu_state == CpuStateBootloader || event->cpu_state == CpuStateKernelInit || event->cpu_state == CpuStateOnline) {
        instance->host_restarting = false;
    }
    if(event->state == HostStateStarting && last.state != HostStateStarting) {
        cpu_app_show_self_check(instance);
    } else if(event->cpu_state == CpuStateRestarting && last.cpu_state != CpuStateRestarting) {
        instance->host_restarting = true;
        cpu_app_show_self_check(instance);
    } else if(event->state != last.state) {
        cpu_app_show_host_state(instance);
    }
}

// Host thread: hand the event over to the app's event loop, in order.
static void cpu_app_host_event_callback(const void* message, void* context) {
    CpuApp* instance = context;
    CpuAppMessage app_message = {.type = CpuAppMessageTypeHostEvent, .as.host_event = *(const HostEvent*)message};
    if(furi_message_queue_put(instance->app_queue, &app_message, 0) != FuriStatusOk) {
        FURI_LOG_E(TAG, "Host event dropped: queue full");
    }
}

// Only while the self check screen is up, so it is cheap enough
static bool cpu_app_frame_is_black(const uint8_t* data, size_t size) {
    for(size_t i = 0; i < size; i++) {
        if(data[i]) return false;
    }
    return true;
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
        case CpuAppMessageTypeStart: {
            host_power_on(instance->host);
            // A host powered already publishes nothing: take its state as it is
            HostEvent event = {.state = host_get_state(instance->host), .cpu_state = instance->host_event.cpu_state};
            cpu_app_host_event(instance, &event);
            break;
        }
        case CpuAppMessageTypeReset:
            instance->host_restarting = true;
            host_power_restart(instance->host);
            // The host stays Starting across a restart: no event brings it up
            cpu_app_show_self_check(instance);
            break;
        case CpuAppMessageTypeHostEvent:
            cpu_app_host_event(instance, &message.as.host_event);
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
            if(instance->host_restarting) {
                // A frame of the kernel that went down
            } else if(instance->self_check_shown && cpu_app_frame_is_black(message.as.new_frame.data, message.as.new_frame.size)) {
                // Not drawn yet: keep the self check screen
            } else {
                instance->self_check_shown = false;
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
    instance->self_check = self_check_screen_alloc();
    instance->app_queue = furi_message_queue_alloc(CPU_APP_MESSAGE_QUEUE_SIZE, sizeof(CpuAppMessage));

    instance->pio_get_frame = pio_get_frame_init(&gpio_cpu_spi_cs, &gpio_cpu_spi_sck, &gpio_cpu_spi_mosi);
    pio_get_frame_set_callback_rx(instance->pio_get_frame, cpu_app_pio_get_frame_isr, instance);

    furi_event_loop_subscribe_message_queue(instance->event_loop, instance->app_queue, FuriEventLoopEventIn, cpu_app_message_logic, instance);
    instance->host_event = (HostEvent){.state = HostStateOff, .cpu_state = CpuStateUnknown};
    instance->host_subscription = furi_pubsub_subscribe(host_get_pubsub(instance->host), cpu_app_host_event_callback, instance);

    instance->display_view = view_alloc();
    view_allocate_model(instance->display_view, ViewModelTypeLockFree, sizeof(CpuAppModel));
    cpu_app_model_apply(instance, cpu_app_model_init, instance->self_check);

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
    furi_record_close(RECORD_HOST);
    furi_event_loop_unsubscribe(instance->event_loop, instance->app_queue);
    furi_event_loop_free(instance->event_loop);
    furi_message_queue_free(instance->app_queue);

    pio_get_frame_deinit(instance->pio_get_frame);
    self_check_screen_free(instance->self_check);

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
