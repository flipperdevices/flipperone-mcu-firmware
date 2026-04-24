#include "led.h"

#include <furi_bsp.h>
#include <furi_hal_nvm.h>
#include <furi_hal_resources.h>
#include <api_lock.h>
#include <drivers/ws2812/ws2812.h>

#define TAG "Led"

#define LED_LINE_1_LEDS_COUNT (4U)
#define LED_LINE_2_LEDS_COUNT (7U)
#define LED_LINE_3_LEDS_COUNT (6U)
#define LED_TOTAL_LEDS_COUNT  (LED_LINE_1_LEDS_COUNT + LED_LINE_2_LEDS_COUNT + LED_LINE_3_LEDS_COUNT)

#define LED_MAX_MESSAGES            (8U)
#define LED_WAIT_POWER_ON_WS2812_MS (5U)
#define LED_LINE1_INDEX             (0U)
#define LED_LINE2_INDEX             (1U)
#define LED_LINE3_INDEX             (2U)
#define LED_LINES_COUNT             (3U)

#define LED_BRIGHTNESS_MULTIPLIER (0.5f)

typedef struct {
    uint32_t line[LED_TOTAL_LEDS_COUNT];
    LedColor colors[LED_TOTAL_LEDS_COUNT];
    StatusLedPower mask_power;
    FuriState* brightness[LedGroupMax];
} LedState;

struct Led {
    FuriEventLoop* event_loop;
    FuriMessageQueue* message_queue;
    Ws2812* ws2812;
    LedState led_state;
};

typedef enum {
    LedMessageTypeSetColorSingle,
    LedMessageTypeSetColorBatch,
    LedMessageTypeSetBrightness,
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
    LedGroup group;
    uint8_t brightness;
} LedMessageSetBrightness;

typedef struct {
    LedMessageType type;
    FuriApiLock lock;
    bool* result;
    union {
        LedMessageSetColorSingle set_color_single;
        LedMessageSetColorBatch set_color_batch;
        LedMessageSetBrightness set_brightness;
    };
} LedMessage;

static const char* led_group_names[LedGroupMax] = {
    [LedGroupLink] = "LedGroupLinkBrightness",
    [LedGroupPower] = "LedGroupPowerBrightness",
    [LedGroupWattmeter] = "LedGroupWattmeterBrightness",
};

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

static uint32_t* led_get_line_1(Led* instance) {
    return instance->led_state.line;
}

static uint32_t* led_get_line_2(Led* instance) {
    return instance->led_state.line + LED_LINE_1_LEDS_COUNT;
}

static uint32_t* led_get_line_3(Led* instance) {
    return instance->led_state.line + LED_LINE_1_LEDS_COUNT + LED_LINE_2_LEDS_COUNT;
}

static void led_update_lines(Led* instance, LedUpdateLine update_line) {
    furi_assert(instance);
    if(update_line & LedUpdateLine1) {
        if(led_start_off_timer(instance, led_line_is_wanna_power(led_get_line_1(instance), LED_LINE_1_LEDS_COUNT), StatusLedPowerLine1)) {
            ws2812_write_buffer_dma(instance->ws2812, LED_LINE1_INDEX, led_get_line_1(instance), LED_LINE_1_LEDS_COUNT);
        }
    }
    if(update_line & LedUpdateLine2) {
        if(led_start_off_timer(instance, led_line_is_wanna_power(led_get_line_2(instance), LED_LINE_2_LEDS_COUNT), StatusLedPowerLine2)) {
            ws2812_write_buffer_dma(instance->ws2812, LED_LINE2_INDEX, led_get_line_2(instance), LED_LINE_2_LEDS_COUNT);
        }
    }
    if(update_line & LedUpdateLine3) {
        if(led_start_off_timer(instance, led_line_is_wanna_power(led_get_line_3(instance), LED_LINE_3_LEDS_COUNT), StatusLedPowerLine3)) {
            ws2812_write_buffer_dma(instance->ws2812, LED_LINE3_INDEX, led_get_line_3(instance), LED_LINE_3_LEDS_COUNT);
        }
    }
}

static LedUpdateLine led_get_update_line_by_type(LedType type) {
    switch(type) {
    // Line 1
    case LedTypeLine1Off:
    case LedTypeNet ... LedTypeEth1:
        return LedUpdateLine1;

    // Line 2
    case LedTypeLine2Off:
    case LedTypePower ... LedTypeBatteryWatt4:
        return LedUpdateLine2;

    // Line 3
    case LedTypeLine3Off:
    case LedTypeUsbCharging ... LedTypeBatteryCenter:
        return LedUpdateLine3;

    // All lines
    case LedTypeLineAllOff:
        return LedUpdateLine1 | LedUpdateLine2 | LedUpdateLine3;
    default:
        return 0;
    }
}

static LedGroup led_get_led_group_by_led_type(LedType type) {
    switch(type) {
    case LedTypeNet ... LedTypeEth1:
        return LedGroupLink;
    case LedTypePower:
        return LedGroupPower;
    case LedTypeBatteryOutline ... LedTypeBatteryCenter:
        return LedGroupWattmeter;
    default:
        furi_crash("LedType");
    }
}

static const LedType leds_in_link_group[] = {
    LedTypeNet,
    LedTypeWiFi,
    LedTypeEth2,
    LedTypeEth1,
};

static const LedType leds_in_power_group[] = {
    LedTypePower,
};

static const LedType leds_in_wattmeter_group[] = {
    LedTypeBatteryOutline,
    LedTypeBatteryOutline + 1, // second led of the outline
    LedTypeBatteryWatt1,
    LedTypeBatteryWatt2,
    LedTypeBatteryWatt3,
    LedTypeBatteryWatt4,
    LedTypeUsbCharging,
    LedTypeUsbWatt1,
    LedTypeUsbWatt2,
    LedTypeUsbWatt3,
    LedTypeUsbWatt4,
    LedTypeBatteryCenter,
};

static const LedType* leds_group[] = {
    [LedGroupLink] = leds_in_link_group,
    [LedGroupPower] = leds_in_power_group,
    [LedGroupWattmeter] = leds_in_wattmeter_group,
};

const size_t leds_in_group_count[] = {
    [LedGroupLink] = COUNT_OF(leds_in_link_group),
    [LedGroupPower] = COUNT_OF(leds_in_power_group),
    [LedGroupWattmeter] = COUNT_OF(leds_in_wattmeter_group),
};

static void led_set_color(Led* instance, LedType type, LedColor color, uint8_t brightness) {
    instance->led_state.colors[type] = color;

    LedColor adjusted_color = {
        ((float)color.r * brightness) / 255.0f * LED_BRIGHTNESS_MULTIPLIER,
        ((float)color.g * brightness) / 255.0f * LED_BRIGHTNESS_MULTIPLIER,
        ((float)color.b * brightness) / 255.0f * LED_BRIGHTNESS_MULTIPLIER,
    };
    instance->led_state.line[type] = ws2812_urgb_u32(adjusted_color.r, adjusted_color.g, adjusted_color.b);
}

static void led_process_set_color_batch(Led* instance, LedItem* items, size_t count) {
    LedUpdateLine update_line = 0;
    for(size_t i = 0; i < count; i++) {
        switch(items[i].type) {
        // regular leds
        case LedTypeNet ... LedTypeBatteryCenter:
            LedGroup group = led_get_led_group_by_led_type(items[i].type);

            uint8_t brightness;
            furi_state_get(instance->led_state.brightness[group], &brightness);

            led_set_color(instance, items[i].type, items[i].color, brightness);
            if(items[i].type == LedTypeBatteryOutline) {
                // outline is 2 leds
                led_set_color(instance, LedTypeBatteryOutline + 1, items[i].color, brightness);
            }
            break;
        case LedTypeLine1Off:
            //turn off line 1
            memset(led_get_line_1(instance), 0x00, LED_LINE_1_LEDS_COUNT * sizeof(instance->led_state.line[0]));
            break;
        case LedTypeLine2Off:
            //turn off line 2
            memset(led_get_line_2(instance), 0x00, LED_LINE_2_LEDS_COUNT * sizeof(instance->led_state.line[0]));
            break;
        case LedTypeLine3Off:
            //turn off line 3
            memset(led_get_line_3(instance), 0x00, LED_LINE_3_LEDS_COUNT * sizeof(instance->led_state.line[0]));
            break;
        case LedTypeLineAllOff:
            //turn off all lines
            memset(instance->led_state.line, 0x00, sizeof(instance->led_state.line));
            break;
        }

        update_line |= led_get_update_line_by_type(items[i].type);
    }

    led_update_lines(instance, update_line);
}

void led_process_set_brightness(Led* instance, LedGroup group, uint8_t brightness) {
    furi_assert(instance);
    furi_check(group < LedGroupMax);

    const LedType* group_leds = leds_group[group];
    size_t group_leds_count = leds_in_group_count[group];
    LedUpdateLine update_line = 0;

    for(size_t i = 0; i < group_leds_count; i++) {
        LedType type = group_leds[i];
        led_set_color(instance, type, instance->led_state.colors[type], brightness);
        update_line |= led_get_update_line_by_type(type);
    }

    led_update_lines(instance, update_line);

    furi_hal_nvm_set_uint32(led_group_names[group], brightness);
    furi_state_set(instance->led_state.brightness[group], &brightness);
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
    case LedMessageTypeSetBrightness:
        led_process_set_brightness(instance, msg.set_brightness.group, msg.set_brightness.brightness);
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
    GpioPin ws2812_pins[] = {gpio_status_led_line1, gpio_status_led_line2, gpio_status_led_line3};
    instance->ws2812 = ws2812_init(ws2812_pins, LED_LINES_COUNT);
    instance->event_loop = furi_event_loop_alloc();
    instance->message_queue = furi_message_queue_alloc(LED_MAX_MESSAGES, sizeof(LedMessage));

    for(size_t i = 0; i < LedGroupMax; i++) {
        instance->led_state.brightness[i] = furi_state_alloc(sizeof(uint8_t));
        const char* state_name = led_group_names[i];

        uint32_t brightness_value;
        if(furi_hal_nvm_get_uint32(state_name, &brightness_value) != FuriHalNvmStorageOK) {
            FURI_LOG_E(TAG, "Failed to read %s from NVM, defaulting to 255", state_name);
            brightness_value = 255;
            furi_hal_nvm_set_uint32(state_name, brightness_value);
        }

        uint8_t brightness_state = (uint8_t)brightness_value;
        furi_state_set(instance->led_state.brightness[i], &brightness_state);
    }

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

void led_set_brightness(Led* instance, LedGroup group, uint8_t brightness) {
    furi_check(instance);

    const LedMessage msg = {
        .type = LedMessageTypeSetBrightness,

        .set_brightness =
            {
                .group = group,
                .brightness = brightness,
            },

    };
    led_send_message(instance, &msg);
}

FuriState* led_get_link_brightness_state(Led* instance) {
    furi_check(instance);
    return instance->led_state.brightness[LedGroupLink];
}

FuriState* led_get_power_brightness_state(Led* instance) {
    furi_check(instance);
    return instance->led_state.brightness[LedGroupPower];
}

FuriState* led_get_wattmeter_brightness_state(Led* instance) {
    furi_check(instance);
    return instance->led_state.brightness[LedGroupWattmeter];
}
