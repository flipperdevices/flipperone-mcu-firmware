#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_pwm.h>
#include <drivers/display/display_jd9853_spi.h>
#include <drivers/display/display_jd9853_reg.h>
#include <furi_hal_resources.h>
#include <drivers/ws2812/ws2812.h>
#include <gui_test/app_common.h>

#define TAG "GuiTest"

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

extern App app_test_keypad;
extern App app_test_touchpad;
extern App app_playdate;

App* apps[] = {
    &app_test_keypad,
    &app_test_touchpad,
    &app_playdate,
};

static void app_call_render(App* app) {
    if(app) {
        if(app->render) {
            app->render(app);
        }
    }
}

static bool app_call_input(App* app, const GuiTestMessage* message) {
    bool handled = false;
    if(app) {
        if(app->input) {
            handled = app->input(app, message);
        }
    }
    return handled;
}

static void app_call_scroll(App* app) {
    if(app) {
        if(app->scroll) {
            app->scroll(app);
        }
    }
}

int32_t gui_test_app(void* p) {
    FURI_LOG_I(TAG, "Starting GUI Test App");

    Ws2812* ws2812 = ws2812_init(&gpio_status_led_line1, 1);
    ws2812_put_pixel_rgb(ws2812, 0, 10, 0, 0);

    DisplayJd9853SPI* display = display_jd9853_spi_init();
    display_jd9853_spi_set_brightness(display, 10);

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

    while(1) {
        App* app = apps[app_index];
        int32_t last_app_index = app_index;

        Clay_ResetMeasureTextCache();
        Clay_BeginLayout();

        app_call_render(app);

        Clay_RenderCommandArray renderCommands = Clay_EndLayout();

        render_clear_buffer(0x00);

        for(int i = 0; i < renderCommands.length; i++) {
            Clay_RenderCommand* renderCommand = &renderCommands.internalArray[i];
            Clay_BoundingBox boundingBox = renderCommand->boundingBox;

            switch(renderCommand->commandType) {
            case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
                render_rectangle(&boundingBox, &renderCommand->renderData.rectangle);
            } break;
            case CLAY_RENDER_COMMAND_TYPE_BORDER: {
                render_border(&boundingBox, &renderCommand->renderData.border);
            } break;
            case CLAY_RENDER_COMMAND_TYPE_TEXT: {
                render_text(&boundingBox, &renderCommand->renderData.text);
            } break;
            case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
                render_image(&boundingBox, &renderCommand->renderData.image);
            } break;
            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
                render_scissor_start(&boundingBox);
            } break;
            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
                render_scissor_end();
            } break;
            case CLAY_RENDER_COMMAND_TYPE_CUSTOM: {
                furi_crash("Custom render commands are not supported");
            } break;
            }
        }

        display_jd9853_spi_write_buffer(display, JD9853_WIDTH, JD9853_HEIGHT, render_get_buffer(), JD9853_WIDTH * JD9853_HEIGHT);

        GuiTestMessage message;
        if(furi_message_queue_get(queue, &message, 1000 / 100) == FuriStatusOk) {
            bool message_present = true;

            while(message_present) {
                bool handled = app_call_input(app, &message);

                if(!handled) {
                    switch(message.type) {
                    case GuiTestMessageTypeInputEvent: {
                        InputEvent event = message.input_event;
                        if(event.type == InputTypePress) {
                            if(event.key == InputKey1) {
                                app_index = 0;
                            }
                            if(event.key == InputKey2) {
                                app_index = 1;
                            }
                            if(event.key == InputKey3) {
                                app_index = 2;
                            }
                            if(event.key == InputKey4) {
                                app_index = 3;
                            }
                            if(event.key == InputKey5) {
                                app_index = 4;
                            }

                            if(app_index >= COUNT_OF(apps)) {
                                app_index = last_app_index;
                            }
                        }
                    } break;
                    }
                }

                if(furi_message_queue_get(queue, &message, 0) != FuriStatusOk) {
                    message_present = false;
                }
            }
        }

        if(last_app_index == app_index) {
            app_call_scroll(app);
        }
    }

    return 0;
}
