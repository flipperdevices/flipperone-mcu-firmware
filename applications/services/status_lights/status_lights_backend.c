#include "status_lights.h"

#include <input/input.h>
#include <furi_hal_resources.h>
#include <api_lock.h>
#include <drivers/ws2812/ws2812.h>

#define TAG "StatusLights"

#define STATUS_LIGHTS_LINES_1_LED_COUNT       (4U)
#define STATUS_LIGHTS_LINES_2_LED_COUNT       (7U)
#define STATUS_LIGHTS_LINES_3_LED_COUNT       (6U)
#define STATUS_LIGHTS_MAX_MESSAGES            (8U)
#define STATUS_LIGHTS_WAIT_POWER_ON_WS2812_MS (5U)
#define STATUS_LIGHTS_LINE1_INDEX             (0U)
#define STATUS_LIGHTS_LINE2_INDEX             (1U)
#define STATUS_LIGHTS_LINE3_INDEX             (2U)
#define STATUS_LIGHTS_LINE_COUNT              (3U)

typedef struct {
    uint32_t line1[STATUS_LIGHTS_LINES_1_LED_COUNT];
    uint32_t line2[STATUS_LIGHTS_LINES_2_LED_COUNT];
    uint32_t line3[STATUS_LIGHTS_LINES_3_LED_COUNT];
    StatusLedPower mask_power;
} StatusLightsStat;

struct StatusLights {
    FuriEventLoop* event_loop;
    FuriMessageQueue* message_queue;
    StatusLightsStat status_lights_stat;
    Ws2812* ws2812;
};

typedef enum {
    StatusLightsMessageTypeSetColor,
} StatusLightsMessageType;

typedef struct {
    StatusLightsMessageType type;
    FuriApiLock lock;
    bool* result;
    union {
        struct {
            StatusLightsType status_lights_type;
            StatusLightsColor color;
        } set_color;
    };
} StatusLightsMessage;

void status_lights_event_isr(void* context) {
    furi_assert(context);
    StatusLights* instance = (StatusLights*)context;
}

static bool status_lights_check_need_power(uint32_t* line_buffer, size_t led_count) {
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

static FURI_ALWAYS_INLINE bool status_lights_start_off_timer(StatusLights* instance, bool check_line, StatusLedPower line_power) {
    furi_assert(instance);
    if(check_line != (instance->status_lights_stat.mask_power & line_power)) {
        if(check_line) {
            instance->status_lights_stat.mask_power |= line_power;
            input_srv_led_power(instance->status_lights_stat.mask_power);
            furi_delay_ms(STATUS_LIGHTS_WAIT_POWER_ON_WS2812_MS);
        } else {
            instance->status_lights_stat.mask_power &= ~line_power;
            input_srv_led_power(instance->status_lights_stat.mask_power);
        }
    }
    return instance->status_lights_stat.mask_power & line_power;
}

static void status_lights_message_queue_callback(FuriEventLoopObject* object, void* context) {
    furi_assert(context);
    StatusLights* instance = context;
    furi_assert(object == instance->message_queue);

    StatusLightsMessage msg;
    furi_check(furi_message_queue_get(instance->message_queue, &msg, 0) == FuriStatusOk);

    bool result = false;

    switch(msg.type) {
    case StatusLightsMessageTypeSetColor:
        if(msg.set_color.status_lights_type < StatusLightsTypePower) { //line 1
            instance->status_lights_stat.line1[msg.set_color.status_lights_type] =
                ws2812_urgb_u32(msg.set_color.color.r, msg.set_color.color.g, msg.set_color.color.b);

            if(status_lights_start_off_timer(
                   instance, status_lights_check_need_power(instance->status_lights_stat.line1, STATUS_LIGHTS_LINES_1_LED_COUNT), StatusLedPowerLine1))
                ws2812_write_buffer_dma(instance->ws2812, STATUS_LIGHTS_LINE1_INDEX, instance->status_lights_stat.line1, STATUS_LIGHTS_LINES_1_LED_COUNT);

        } else if(msg.set_color.status_lights_type < StatusLightsTypeUsbCharging) { //line 2
            instance->status_lights_stat.line2[msg.set_color.status_lights_type - StatusLightsTypePower] =
                ws2812_urgb_u32(msg.set_color.color.r, msg.set_color.color.g, msg.set_color.color.b);
            if(msg.set_color.status_lights_type == StatusLightsTypeBatteryOutline) {
                //outline is 2 leds
                instance->status_lights_stat.line2[msg.set_color.status_lights_type - StatusLightsTypePower + 1] =
                    ws2812_urgb_u32(msg.set_color.color.r, msg.set_color.color.g, msg.set_color.color.b);
            }

            if(status_lights_start_off_timer(
                   instance, status_lights_check_need_power(instance->status_lights_stat.line2, STATUS_LIGHTS_LINES_2_LED_COUNT), StatusLedPowerLine2))
                ws2812_write_buffer_dma(instance->ws2812, STATUS_LIGHTS_LINE2_INDEX, instance->status_lights_stat.line2, STATUS_LIGHTS_LINES_2_LED_COUNT);
        } else if(msg.set_color.status_lights_type < StatusLightsTypeLine1Off) { //line 3
            instance->status_lights_stat.line3[msg.set_color.status_lights_type - StatusLightsTypeUsbCharging] =
                ws2812_urgb_u32(msg.set_color.color.r, msg.set_color.color.g, msg.set_color.color.b);

            if(status_lights_start_off_timer(
                   instance, status_lights_check_need_power(instance->status_lights_stat.line3, STATUS_LIGHTS_LINES_3_LED_COUNT), StatusLedPowerLine3))
                ws2812_write_buffer_dma(instance->ws2812, STATUS_LIGHTS_LINE3_INDEX, instance->status_lights_stat.line3, STATUS_LIGHTS_LINES_3_LED_COUNT);
        } else {
            switch(msg.set_color.status_lights_type) {
            case StatusLightsTypeLine1Off:
                //turn off line 1
                memset(&instance->status_lights_stat.line1, 0x00, sizeof(instance->status_lights_stat.line1));
                instance->status_lights_stat.mask_power &= ~StatusLedPowerLine1;
                break;
            case StatusLightsTypeLine2Off:
                //turn off line 2
                memset(&instance->status_lights_stat.line2, 0x00, sizeof(instance->status_lights_stat.line2));
                instance->status_lights_stat.mask_power &= ~StatusLedPowerLine2;
                break;
            case StatusLightsTypeLine3Off:
                //turn off line 3
                memset(&instance->status_lights_stat.line3, 0x00, sizeof(instance->status_lights_stat.line3));
                instance->status_lights_stat.mask_power &= ~StatusLedPowerLine3;
                break;
            case StatusLightsTypeLineAllOff:
                //turn off all lines
                memset(&instance->status_lights_stat.line1, 0x00, sizeof(instance->status_lights_stat.line1));
                memset(&instance->status_lights_stat.line2, 0x00, sizeof(instance->status_lights_stat.line2));
                memset(&instance->status_lights_stat.line3, 0x00, sizeof(instance->status_lights_stat.line3));
                instance->status_lights_stat.mask_power &= ~(StatusLedPowerLine1 | StatusLedPowerLine2 | StatusLedPowerLine3);
                break;
            default:
                furi_crash();
                break;
            }
            input_srv_led_power(instance->status_lights_stat.mask_power);
        }
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

static void status_lights_send_message(StatusLights* instance, const StatusLightsMessage* message) {
    furi_check(furi_message_queue_put(instance->message_queue, message, FuriWaitForever) == FuriStatusOk);

    if(message->lock) {
        api_lock_wait_unlock_and_free(message->lock);
    }
}

static StatusLights* status_lights_alloc(void) {
    StatusLights* instance = (StatusLights*)malloc(sizeof(StatusLights));

    // Ws2812 init
    GpioPin* ws2812_pins = (GpioPin*)malloc(sizeof(GpioPin) * STATUS_LIGHTS_LINE_COUNT);
    ws2812_pins[STATUS_LIGHTS_LINE1_INDEX] = gpio_status_led_line1;
    ws2812_pins[STATUS_LIGHTS_LINE2_INDEX] = gpio_status_led_line2;
    ws2812_pins[STATUS_LIGHTS_LINE3_INDEX] = gpio_status_led_line3;
    instance->ws2812 = ws2812_init(ws2812_pins, STATUS_LIGHTS_LINE_COUNT);
    free(ws2812_pins);

    //Todo: wait startup input service for led power enable
    FuriPubSub* input = furi_record_open(RECORD_INPUT_EVENTS);
    furi_record_close(RECORD_INPUT_EVENTS);

    instance->event_loop = furi_event_loop_alloc();
    instance->message_queue = furi_message_queue_alloc(STATUS_LIGHTS_MAX_MESSAGES, sizeof(StatusLightsMessage));

    furi_event_loop_subscribe_message_queue(
        instance->event_loop, instance->message_queue, FuriEventLoopEventIn, status_lights_message_queue_callback, instance);

    furi_record_create(RECORD_STATUS_LIGHTS, instance);

    return instance;
}

int32_t status_lights_srv(void* p) {
    UNUSED(p);

    StatusLights* instance = status_lights_alloc();
    furi_event_loop_run(instance->event_loop);

    return 0;
}

void status_lights_notification(StatusLights* instance, StatusLightsType status_lights_type, StatusLightsColor color) {
    furi_check(instance);

    const StatusLightsMessage msg = {
        .type = StatusLightsMessageTypeSetColor,
        .set_color =
            {
                .status_lights_type = status_lights_type,
                .color = color,
            },
    };
    status_lights_send_message(instance, &msg);
}
