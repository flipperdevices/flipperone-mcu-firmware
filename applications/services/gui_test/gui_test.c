#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_pwm.h>
#include <drivers/display/display_jd9853_spi.h>
#include <drivers/display/display_jd9853_reg.h>
#include <furi_hal_resources.h>
#include <drivers/ws2812/ws2812.h>

#include "apps/apps.h"

#define TAG "GuiTest"

extern App app_switcher;
extern void app_switcher_set_app_index(int index);
extern int app_switcher_get_app_index();
extern void app_switcher_init(App* app);

typedef enum {
    Power,
    Unknown,
    WiFi,
    Lan2,
    Lan1,
    USBPlug,
    USBWatt1,
    USBWatt2,
    USBWatt3,
    USBWatt4,
    BatteryCenter,
    BatteryOutline,
    BatteryWatt1,
    BatteryWatt2,
    BatteryWatt3,
    BatteryWatt4,
    Max,
} LedType;

static void gui_handle_clay_errors(Clay_ErrorData errorData) {
    FURI_LOG_E(TAG, "Clay error: %s", errorData.errorText.chars);
}

static void gui_test_input_events_callback(const void* value, void* context) {
    furi_check(value);
    furi_check(context);

    FuriMessageQueue* queue = context;
    const InputEvent* event = value;

    GuiTestMessage message;
    message.type = GuiTestMessageTypeInputEvent;
    message.input_event = *event;

    furi_message_queue_put(queue, &message, FuriWaitForever);
}

static void gui_test_input_touch_events_callback(const void* value, void* context) {
    furi_check(value);
    furi_check(context);

    FuriMessageQueue* queue = context;
    const InputTouchEvent* event = value;

    GuiTestMessage message;
    message.type = GuiTestMessageTypeInputTouchEvent;
    message.input_touch_event = *event;

    furi_message_queue_put(queue, &message, FuriWaitForever);
}

int32_t gui_test_app(void* p) {
    FURI_LOG_I(TAG, "Starting GUI Test App");

    Ws2812* ws2812 = ws2812_init(&gpio_status_led_line1, 1);
    ws2812_put_pixel_rgb(ws2812, 0, 10, 0, 0);

    DisplayJd9853SPI* display = display_jd9853_spi_init();
    display_jd9853_spi_set_brightness(display, 40);

    FuriMessageQueue* queue = furi_message_queue_alloc(32, sizeof(GuiTestMessage));

    FuriPubSub* input = furi_record_open(RECORD_INPUT_EVENTS);
    FuriPubSubSubscription* input_subscription = furi_pubsub_subscribe(input, gui_test_input_events_callback, queue);

    FuriPubSub* input_touch = furi_record_open(RECORD_INPUT_TOUCH_EVENTS);
    FuriPubSubSubscription* input_touch_subscription = furi_pubsub_subscribe(input_touch, gui_test_input_touch_events_callback, queue);

    Clay_SetMaxElementCount(256);
    Clay_SetMaxMeasureTextCacheWordCount(1024);

    uint64_t totalMemorySize = Clay_MinMemorySize();
    FURI_LOG_I(TAG, "Clay allocation: %lluk", totalMemorySize / 1024);

    Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(totalMemorySize, malloc(totalMemorySize));
    Clay_Initialize(arena, (Clay_Dimensions){JD9853_WIDTH, JD9853_HEIGHT}, (Clay_ErrorHandler){gui_handle_clay_errors});
    Clay_SetMeasureTextFunction(render_measure_text, NULL);

    int32_t app_index = 0;

    RenderBuffer* buffer = render_alloc_buffer();

    render_set_current_buffer(buffer);

    app_switcher_init(&app_switcher);
    bool switching = false;

    while(1) {
        App* app = apps[app_index];
        int32_t last_app_index = app_index;

        Clay_ResetMeasureTextCache();
        Clay_BeginLayout();

        if(switching) {
            apps_call_render(&app_switcher);
        } else {
            apps_call_render(app);
        }

        Clay_RenderCommandArray renderCommands = Clay_EndLayout();

        render_clear_buffer(0x00);
        render_do_render(&renderCommands);

        size_t width = render_get_buffer_width(buffer);
        size_t height = render_get_buffer_height(buffer);
        display_jd9853_spi_write_buffer(display, width, height, render_get_buffer_data(buffer), width * height);

        GuiTestMessage message;
        if(furi_message_queue_get(queue, &message, 1000 / 60) == FuriStatusOk) {
            bool message_present = true;

            while(message_present) {
                bool handled = false;

                if(switching) {
                    handled = apps_call_input(&app_switcher, &message);
                } else {
                    handled = apps_call_input(app, &message);
                }

                if(!handled) {
                    switch(message.type) {
                    case GuiTestMessageTypeInputEvent: {
                        InputEvent event = message.input_event;
                        if(event.type == InputTypePress) {
                            if(event.key == InputKeySw) {
                                if(!switching) {
                                    app_switcher_set_app_index(app_index);
                                    switching = true;
                                } else if(switching) {
                                    app_index = app_switcher_get_app_index();
                                    switching = false;
                                }
                            }

                            if(event.key == InputKeyOk) {
                                if(switching) {
                                    app_index = app_switcher_get_app_index();
                                    switching = false;
                                }
                            }
                        }
                    } break;
                    case GuiTestMessageTypeInputTouchEvent:

                        break;
                    }
                }

                if(furi_message_queue_get(queue, &message, 0) != FuriStatusOk) {
                    message_present = false;
                }
            }
        }

        if(!switching && (last_app_index == app_index)) {
            apps_call_scroll(app);
        }
    }

    return 0;
}
