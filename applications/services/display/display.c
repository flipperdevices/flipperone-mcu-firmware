#include "display.h"
#include <furi.h>
#include <drivers/display/display_jd9853_qspi.h>
#include <drivers/spi_get_frame/spi_get_frame.h>
#include <api_lock.h>
#include <pico.h>

#define TAG "Display"

#define DISPLAY_MAX_MESSAGES   (8)
#define DISPLAY_BRIGHTNESS_MIN (0)
#define DISPLAY_BRIGHTNESS_MAX (100)

typedef enum {
    DisplayEventTypeSpiFrameReady = (1 << 0),
    DisplayEventTypeAll = (DisplayEventTypeSpiFrameReady),
} DisplayEventType;

struct Display {
    FuriEventLoop* event_loop;
    FuriPubSub* event_pubsub;
    DisplayJd9853QSPI* display_header;
    SpiGetFrame* spi_get_frame;
    uint8_t* spi_get_frame_data_ptr;
    size_t spi_get_frame_data_size;
    DisplayMode mode;
    FuriMessageQueue* message_queue;
};

typedef enum {
    DisplayMessageTypeSetBrightness,
    DisplayMessageTypeGetBrightness,
    DisplayMessageTypeSetMode,
    DisplayMessageTypeGetMode,
} DisplayMessageType;

typedef struct {
    DisplayMessageType type;
    FuriApiLock lock;
    bool* result;
    union {
        int8_t* get_brightness;
        int8_t set_brightness;
        DisplayMode* get_mode;
        DisplayMode set_mode;
    };
} DisplayMessage;

static void __isr __not_in_flash_func(display_spi_get_frame_isr)(uint8_t* data, size_t size, void* context) {
    Display* instance = (Display*)context;
    instance->spi_get_frame_data_ptr = data;
    instance->spi_get_frame_data_size = size;
    furi_event_loop_set_custom_event(instance->event_loop, DisplayEventTypeSpiFrameReady);
}

void display_event_isr(void* context) {
    furi_assert(context);
    Display* instance = (Display*)context;
}

static void display_message_queue_callback(FuriEventLoopObject* object, void* context) {
    furi_assert(context);
    Display* instance = context;
    furi_assert(object == instance->message_queue);

    DisplayMessage msg;
    furi_check(furi_message_queue_get(instance->message_queue, &msg, 0) == FuriStatusOk);

    bool result = false;

    switch(msg.type) {
    case DisplayMessageTypeSetBrightness:
        display_jd9853_qspi_set_brightness(instance->display_header, msg.set_brightness);
        break;
    case DisplayMessageTypeGetBrightness:
        *(msg.get_brightness) = display_jd9853_qspi_get_brightness(instance->display_header);
        break;
    case DisplayMessageTypeSetMode:
        instance->mode = msg.set_mode;
        break;
    case DisplayMessageTypeGetMode:
        *(msg.get_mode) = instance->mode;
        break;
    default:
        furi_crash("Invalid message type");
        break;
    }

    if(msg.result) {
        *msg.result = result;
    }

    if(msg.lock) {
        api_lock_unlock(msg.lock);
    }
}

static void display_custom_event_callback(uint32_t events, void* context) {
    furi_assert(context);
    Display* instance = (Display*)context;

    if(events & DisplayEventTypeSpiFrameReady) {
        display_jd9853_qspi_write_buffer(instance->display_header, instance->spi_get_frame_data_ptr, instance->spi_get_frame_data_size);
    }
}

static void display_send_message(Display* instance, const DisplayMessage* message) {
    furi_check(furi_message_queue_put(instance->message_queue, message, FuriWaitForever) == FuriStatusOk);

    if(message->lock) {
        api_lock_wait_unlock_and_free(message->lock);
    }
}

static Display* display_alloc(void) {
    Display* instance = (Display*)malloc(sizeof(Display));
    instance->event_loop = furi_event_loop_alloc();
    instance->message_queue = furi_message_queue_alloc(DISPLAY_MAX_MESSAGES, sizeof(DisplayMessage));
    instance->mode = DisplayModeCpu;
    instance->display_header = display_jd9853_qspi_init();
    instance->spi_get_frame = spi_get_frame_init();

    furi_event_loop_subscribe_message_queue(instance->event_loop, instance->message_queue, FuriEventLoopEventIn, display_message_queue_callback, instance);

    furi_event_loop_set_custom_event_callback(instance->event_loop, display_custom_event_callback, instance);

    display_jd9853_qspi_set_brightness(instance->display_header, 5); // Set backlight to 5%
    spi_get_frame_set_callback_rx(instance->spi_get_frame, display_spi_get_frame_isr, instance);

    instance->event_pubsub = furi_pubsub_alloc();
    furi_record_create(RECORD_DISPLAY, instance);

    return instance;
}

int32_t display_srv(void* p) {
    UNUSED(p);

    Display* instance = display_alloc();
    furi_event_loop_run(instance->event_loop);

    return 0;
}

void display_set_brightness(Display* instance, int8_t brightness) {
    furi_check(instance);

    if(brightness < DISPLAY_BRIGHTNESS_MIN) brightness = DISPLAY_BRIGHTNESS_MIN;
    if(brightness > DISPLAY_BRIGHTNESS_MAX) brightness = DISPLAY_BRIGHTNESS_MAX;

    const DisplayMessage msg = {
        .type = DisplayMessageTypeSetBrightness,
        .set_brightness = brightness,
    };

    display_send_message(instance, &msg);
}

int8_t display_get_brightness(Display* instance) {
    furi_check(instance);

    int8_t brightness;
    DisplayMessage msg = {
        .type = DisplayMessageTypeGetBrightness,
        .get_brightness = &brightness,
        .lock = api_lock_alloc_locked(),
    };

    display_send_message(instance, &msg);

    return brightness;
}

void display_set_mode(Display* instance, DisplayMode mode) {
    furi_check(instance);
    furi_check(mode < DisplayModeCount);
    const DisplayMessage msg = {
        .type = DisplayMessageTypeSetMode,
        .set_mode = mode,
    };

    display_send_message(instance, &msg);
}

DisplayMode display_get_mode(Display* instance) {
    furi_check(instance);

    DisplayMode mode;
    DisplayMessage msg = {
        .type = DisplayMessageTypeGetMode,
        .get_mode = &mode,
        .lock = api_lock_alloc_locked(),
    };

    display_send_message(instance, &msg);

    return mode;
}

FuriPubSub* display_get_pubsub(Display* display) {
    furi_check(display);
    return display->event_pubsub;
}
