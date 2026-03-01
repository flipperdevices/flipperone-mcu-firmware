#include "led.h"

#include <furi_bsp.h>
#include <furi_hal_resources.h>
#include <api_lock.h>
#include <drivers/ws2812/ws2812.h>

#define TAG "Led"

#define LED_LINE_1_LEDS_COUNT       (4U)
#define LED_LINE_2_LEDS_COUNT       (7U)
#define LED_LINE_3_LEDS_COUNT       (6U)
#define LED_MAX_MESSAGES            (8U)
#define LED_WAIT_POWER_ON_WS2812_MS (5U)
#define LED_LINE1_INDEX             (0U)
#define LED_LINE2_INDEX             (1U)
#define LED_LINE3_INDEX             (2U)
#define LED_LINES_COUNT             (3U)

typedef struct {
    uint32_t line1[LED_LINE_1_LEDS_COUNT];
    uint32_t line2[LED_LINE_2_LEDS_COUNT];
    uint32_t line3[LED_LINE_3_LEDS_COUNT];
    StatusLedPower mask_power;
} LedState;

struct Led {
    FuriEventLoop* event_loop;
    FuriMessageQueue* message_queue;
    LedState led_state;
    Ws2812* ws2812;
};

typedef enum {
    LedMessageTypeSetColorSingle,
    LedMessageTypeSetColorBatch,
} LedMessageType;

typedef enum {
    LedUpdateLine1 = (1 << 0U),
    LedUpdateLine2 = (1 << 1U),
    LedUpdateLine3 = (1 << 2U),
} LedUpdateLine;

typedef struct {
    LedType type;
    LedColor color;
} LedMessageSetColorSingle;

typedef struct {
    const LedBatch* items;
} LedMessageSetColorBatch;

typedef struct {
    LedMessageType type;
    FuriApiLock lock;
    bool* result;
    union {
        LedMessageSetColorSingle set_color_single;
        LedMessageSetColorBatch set_color_batch;
    };
} LedMessage;

static bool led_line_is_wanna_power(uint32_t* line_buffer, size_t led_count) {
    furi_assert(line_buffer);
    bool need_power = false;
    for(size_t i = 0; i < led_count; i++) {
        if(line_buffer[i] != 0) {
            need_power = true;
            break;
        }
    }
    return need_power;
}

static FURI_ALWAYS_INLINE bool led_start_off_timer(Led* instance, bool check_line, StatusLedPower line_power) {
    furi_assert(instance);
    if(check_line != (instance->led_state.mask_power & line_power)) {
        if(check_line) {
            instance->led_state.mask_power |= line_power;
            furi_bsp_expander_control_led_power(instance->led_state.mask_power);
            furi_delay_ms(LED_WAIT_POWER_ON_WS2812_MS);
        } else {
            instance->led_state.mask_power &= ~line_power;
            furi_bsp_expander_control_led_power(instance->led_state.mask_power);
        }
    }
    return instance->led_state.mask_power & line_power;
}

static void led_process_set_color_batch(Led* instance, LedItem* items, size_t count) {
    LedUpdateLine update_line = 0;
    for(size_t i = 0; i < count; i++) {
        switch(items[i].type) {
        // line 1
        case LedTypeNet ... LedTypeEth1:
            instance->led_state.line1[items[i].type] = ws2812_urgb_u32(items[i].color.r, items[i].color.g, items[i].color.b);
            update_line |= LedUpdateLine1;
            break;
        // line 2
        case LedTypePower ... LedTypeBatteryWatt4:
            instance->led_state.line2[items[i].type - LedTypePower] = ws2812_urgb_u32(items[i].color.r, items[i].color.g, items[i].color.b);
            if(items[i].type == LedTypeBatteryOutline) {
                //outline is 2 leds
                instance->led_state.line2[items[i].type - LedTypePower + 1] = ws2812_urgb_u32(items[i].color.r, items[i].color.g, items[i].color.b);
            }
            update_line |= LedUpdateLine2;
            break;
        // line 3
        case LedTypeUsbCharging ... LedTypeBatteryCenter:
            instance->led_state.line3[items[i].type - LedTypeUsbCharging] = ws2812_urgb_u32(items[i].color.r, items[i].color.g, items[i].color.b);
            update_line |= LedUpdateLine3;
            break;
        case LedTypeLine1Off:
            //turn off line 1
            memset(&instance->led_state.line1, 0x00, sizeof(instance->led_state.line1));
            update_line |= LedUpdateLine1;
            break;
        case LedTypeLine2Off:
            //turn off line 2
            memset(&instance->led_state.line2, 0x00, sizeof(instance->led_state.line2));
            update_line |= LedUpdateLine2;
            break;
        case LedTypeLine3Off:
            //turn off line 3
            memset(&instance->led_state.line3, 0x00, sizeof(instance->led_state.line3));
            update_line |= LedUpdateLine3;
            break;
        case LedTypeLineAllOff:
            //turn off all lines
            memset(&instance->led_state.line1, 0x00, sizeof(instance->led_state.line1));
            memset(&instance->led_state.line2, 0x00, sizeof(instance->led_state.line2));
            memset(&instance->led_state.line3, 0x00, sizeof(instance->led_state.line3));
            update_line |= (LedUpdateLine1 | LedUpdateLine2 | LedUpdateLine3);
            break;
        }
    }

    if(update_line & LedUpdateLine1) {
        if(led_start_off_timer(instance, led_line_is_wanna_power(instance->led_state.line1, LED_LINE_1_LEDS_COUNT), StatusLedPowerLine1))
            ws2812_write_buffer_dma(instance->ws2812, LED_LINE1_INDEX, instance->led_state.line1, LED_LINE_1_LEDS_COUNT);
    }
    if(update_line & LedUpdateLine2) {
        if(led_start_off_timer(instance, led_line_is_wanna_power(instance->led_state.line2, LED_LINE_2_LEDS_COUNT), StatusLedPowerLine2))
            ws2812_write_buffer_dma(instance->ws2812, LED_LINE2_INDEX, instance->led_state.line2, LED_LINE_2_LEDS_COUNT);
    }
    if(update_line & LedUpdateLine3) {
        if(led_start_off_timer(instance, led_line_is_wanna_power(instance->led_state.line3, LED_LINE_3_LEDS_COUNT), StatusLedPowerLine3))
            ws2812_write_buffer_dma(instance->ws2812, LED_LINE3_INDEX, instance->led_state.line3, LED_LINE_3_LEDS_COUNT);
    }
}

static void led_message_queue_callback(FuriEventLoopObject* object, void* context) {
    furi_assert(context);
    Led* instance = context;
    furi_assert(object == instance->message_queue);

    LedMessage msg;
    furi_check(furi_message_queue_get(instance->message_queue, &msg, 0) == FuriStatusOk);

    bool result = false;

    switch(msg.type) {
    case LedMessageTypeSetColorSingle:
        LedItem led_item = {
            .type = msg.set_color_single.type,
            .color = msg.set_color_single.color,
        };
        led_process_set_color_batch(instance, &led_item, 1);
        result = true;
        break;
    case LedMessageTypeSetColorBatch:
        const LedItem* items = msg.set_color_batch.items->items;
        const size_t count = msg.set_color_batch.items->count;
        led_process_set_color_batch(instance, (LedItem*)items, count);
        result = true;
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

static void led_send_message(Led* instance, const LedMessage* message) {
    furi_check(furi_message_queue_put(instance->message_queue, message, FuriWaitForever) == FuriStatusOk);

    if(message->lock) {
        api_lock_wait_unlock_and_free(message->lock);
    }
}

static Led* led_alloc(void) {
    Led* instance = (Led*)malloc(sizeof(Led));

    // Ws2812 init
    GpioPin* ws2812_pins = (GpioPin*)malloc(sizeof(GpioPin) * LED_LINES_COUNT);
    ws2812_pins[LED_LINE1_INDEX] = gpio_status_led_line1;
    ws2812_pins[LED_LINE2_INDEX] = gpio_status_led_line2;
    ws2812_pins[LED_LINE3_INDEX] = gpio_status_led_line3;
    instance->ws2812 = ws2812_init(ws2812_pins, LED_LINES_COUNT);
    free(ws2812_pins);

    instance->event_loop = furi_event_loop_alloc();
    instance->message_queue = furi_message_queue_alloc(LED_MAX_MESSAGES, sizeof(LedMessage));

    furi_event_loop_subscribe_message_queue(instance->event_loop, instance->message_queue, FuriEventLoopEventIn, led_message_queue_callback, instance);

    furi_record_create(RECORD_LEDS, instance);

    return instance;
}

int32_t led_srv(void* p) {
    UNUSED(p);

    Led* instance = led_alloc();
    furi_event_loop_run(instance->event_loop);

    return 0;
}

void led_set_color_single(Led* instance, LedType type, LedColor color) {
    furi_check(instance);

    const LedMessage msg = {
        .type = LedMessageTypeSetColorSingle,

        .set_color_single =
            {
                .type = type,
                .color = color,
            },

    };
    led_send_message(instance, &msg);
}

void led_set_color_batch(Led* instance, const LedBatch* items) {
    furi_check(instance);

    const LedMessage msg = {
        .type = LedMessageTypeSetColorBatch,

        .set_color_batch =
            {
                .items = items,
            },

    };
    led_send_message(instance, &msg);
}
