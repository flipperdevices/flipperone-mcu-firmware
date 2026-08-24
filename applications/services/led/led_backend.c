#include "led.h"

#include <furi_bsp.h>
#include <furi_hal_nvm.h>
#include <api_lock.h>
#include <settings/settings.h>
#include <input/input.h>
#include <input_touch/input_touch.h>

#define TAG "Led"

#define LED_MAX_MESSAGES              (8U)
#define LED_RGB_BRIGHTNESS_MULTIPLIER (0.5f)

#define LED_BACKLIGHT_TIME_DEFAULT (10 * 1000)
#define LED_BACKLIGHT_TIME_MAX     ((UINT16_MAX) * 100) // I2C: 16-bit register with 100ms resolution

typedef struct {
    LedColor colors[LED_TOTAL_LEDS_COUNT];
    FuriState* brightness[LedGroupMax];
    FuriState* backlight_time;
    bool backlight_always_on;
} LedState;

struct Led {
    FuriEventLoop* event_loop;
    FuriMessageQueue* message_queue;
    FuriEventLoopTimer* backlight_timer;
    FuriBspLed* bsp_led;
    LedState led_state;
};

typedef enum {
    LedMessageTypeSetColorSingle,
    LedMessageTypeSetColorBatch,
    LedMessageTypeSetBrightness,
    LedMessageTypeSetBacklightTime,
    LedMessageTypeBacklightControl,
    LedMessageTypeBacklightInputToggle,
} LedMessageType;

typedef struct {
    LedMessageType type;
    FuriApiLock lock;
    bool* result;
    union {
        struct {
            FuriBspLedType type;
            LedColor color;
        } set_color_single;

        struct {
            const LedBatch* items;
        } set_color_batch;

        struct {
            LedGroup group;
            uint8_t brightness;
        } set_brightness;

        struct {
            uint32_t timeout_ms;
        } set_backlight_time;

        struct {
            enum {
                LedBacklightTimeFlagPing = (1 << 0U),
                LedBacklightTimeFlagAlwaysOn = (1 << 1U),
            } flags;
        } backlight_time_control;
    };
} LedMessage;

static const struct {
    const char* name;
    uint8_t default_value;
} led_settings[LedGroupMax] = {
    [LedGroupLink] = {SETTINGS_LED_GROUP_LINK, 255},
    [LedGroupPower] = {SETTINGS_LED_GROUP_POWER, 255},
    [LedGroupWattmeter] = {SETTINGS_LED_GROUP_WATTMETER, 255},
    [LedGroupDisplayBacklight] = {SETTINGS_LED_BACKLIGHT, 26}, // 10%
};

static LedGroup led_get_led_group_by_led_type(FuriBspLedType type) {
    switch(type) {
    case FuriBspLedTypeNet ... FuriBspLedTypeEth1:
        return LedGroupLink;
    case FuriBspLedTypePower:
        return LedGroupPower;
    case FuriBspLedTypeBatteryOutline ... FuriBspLedTypeBatteryCenter:
        return LedGroupWattmeter;
    default:
        furi_crash("FuriBspLedType");
    }
}

static const FuriBspLedType leds_in_link_group[] = {
    FuriBspLedTypeNet,
    FuriBspLedTypeWiFi,
    FuriBspLedTypeEth2,
    FuriBspLedTypeEth1,
};

static const FuriBspLedType leds_in_power_group[] = {
    FuriBspLedTypePower,
};

static const FuriBspLedType leds_in_wattmeter_group[] = {
    FuriBspLedTypeBatteryOutline,
    FuriBspLedTypeBatteryWatt1,
    FuriBspLedTypeBatteryWatt2,
    FuriBspLedTypeBatteryWatt3,
    FuriBspLedTypeBatteryWatt4,
    FuriBspLedTypeUsbCharging,
    FuriBspLedTypeUsbWatt1,
    FuriBspLedTypeUsbWatt2,
    FuriBspLedTypeUsbWatt3,
    FuriBspLedTypeUsbWatt4,
    FuriBspLedTypeBatteryCenter,
};

static const FuriBspLedType* leds_group[LedGroupMax] = {
    [LedGroupLink] = leds_in_link_group,
    [LedGroupPower] = leds_in_power_group,
    [LedGroupWattmeter] = leds_in_wattmeter_group,
    [LedGroupDisplayBacklight] = NULL,
};

static const size_t leds_in_group_count[LedGroupMax] = {
    [LedGroupLink] = COUNT_OF(leds_in_link_group),
    [LedGroupPower] = COUNT_OF(leds_in_power_group),
    [LedGroupWattmeter] = COUNT_OF(leds_in_wattmeter_group),
    [LedGroupDisplayBacklight] = 0,
};

static void led_set_color(Led* instance, FuriBspLedType type, LedColor color, uint8_t brightness) {
    instance->led_state.colors[type] = color;

    LedColor adjusted_color = {
        ((float)color.r * brightness) / 255.0f * LED_RGB_BRIGHTNESS_MULTIPLIER,
        ((float)color.g * brightness) / 255.0f * LED_RGB_BRIGHTNESS_MULTIPLIER,
        ((float)color.b * brightness) / 255.0f * LED_RGB_BRIGHTNESS_MULTIPLIER,
    };
    //instance->led_state.line[type] = ws2812_urgb_u32(adjusted_color.r, adjusted_color.g, adjusted_color.b);
    furi_bsp_led_set(instance->bsp_led, type, adjusted_color.r, adjusted_color.g, adjusted_color.b);
}

static void led_backlight_enable(Led* instance, bool enable) {
    uint8_t brightness = 0;
    if(enable) {
        furi_state_get(instance->led_state.brightness[LedGroupDisplayBacklight], &brightness);
    }
    furi_bsp_display_backlight_set_brightness(brightness);
}

static void led_backlight_ping(Led* instance) {
    led_backlight_enable(instance, true);
    uint32_t backlight_time;
    furi_state_get(instance->led_state.backlight_time, &backlight_time);
    if((!instance->led_state.backlight_always_on) && (backlight_time > 0)) {
        furi_event_loop_timer_start(instance->backlight_timer, backlight_time);
    }
}

static void led_process_set_color_batch(Led* instance, LedItem* items, size_t count) {
    for(size_t i = 0; i < count; i++) {
        switch(items[i].type) {
        // regular leds
        case FuriBspLedTypeNet ... FuriBspLedTypeBatteryCenter: {
            LedGroup group = led_get_led_group_by_led_type(items[i].type);

            uint8_t brightness;
            furi_state_get(instance->led_state.brightness[group], &brightness);

            led_set_color(instance, items[i].type, items[i].color, brightness);
            break;
        }

        case FuriBspLedTypeAllOff:
            //turn off all lines
            furi_bsp_led_all_off(instance->bsp_led);
            break;
        }
    }
    furi_bsp_led_update(instance->bsp_led);
}

static void led_process_set_brightness(Led* instance, LedGroup group, uint8_t brightness, bool save_to_nvm) {
    furi_assert(instance);
    furi_check(group < LedGroupMax);
    furi_state_set(instance->led_state.brightness[group], &brightness);

    if(group == LedGroupDisplayBacklight) {
        led_backlight_ping(instance);
    } else {
        const FuriBspLedType* group_leds = leds_group[group];
        size_t group_leds_count = leds_in_group_count[group];

        for(size_t i = 0; i < group_leds_count; i++) {
            FuriBspLedType type = group_leds[i];
            led_set_color(instance, type, instance->led_state.colors[type], brightness);
        }

        furi_bsp_led_update(instance->bsp_led);
    }

    if(save_to_nvm) {
        furi_hal_nvm_set_uint32(led_settings[group].name, brightness);
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
    case LedMessageTypeSetColorSingle: {
        LedItem led_item = {
            .type = msg.set_color_single.type,
            .color = msg.set_color_single.color,
        };
        led_process_set_color_batch(instance, &led_item, 1);
        result = true;
        break;
    }
    case LedMessageTypeSetColorBatch: {
        const LedItem* items = msg.set_color_batch.items->items;
        const size_t count = msg.set_color_batch.items->count;
        led_process_set_color_batch(instance, (LedItem*)items, count);
        result = true;
        break;
    }
    case LedMessageTypeSetBrightness: {
        led_process_set_brightness(instance, msg.set_brightness.group, msg.set_brightness.brightness, true);
        result = true;
        break;
    }
    case LedMessageTypeSetBacklightTime: {
        uint32_t timeout = msg.set_backlight_time.timeout_ms;
        furi_state_set(instance->led_state.backlight_time, &timeout);
        furi_hal_nvm_set_uint32(SETTINGS_LED_BACKLIGHT_TIME, timeout);

        furi_event_loop_timer_stop(instance->backlight_timer);
        if((!instance->led_state.backlight_always_on) && (timeout > 0)) {
            furi_event_loop_timer_start(instance->backlight_timer, timeout);
        }
        result = true;
        break;
    }
    case LedMessageTypeBacklightControl: {
        if(msg.backlight_time_control.flags & LedBacklightTimeFlagAlwaysOn) {
            instance->led_state.backlight_always_on = true;
            led_backlight_enable(instance, true);
        } else {
            instance->led_state.backlight_always_on = false;
            if((msg.backlight_time_control.flags & LedBacklightTimeFlagPing) == 0) {
                led_backlight_enable(instance, false);
            }
        }
        if(msg.backlight_time_control.flags & LedBacklightTimeFlagPing) {
            led_backlight_ping(instance);
        }
        result = true;
        break;
    }
    case LedMessageTypeBacklightInputToggle: {
        led_backlight_ping(instance);
        result = true;
        break;
    }

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

static void led_backlight_timer_callback(void* context) {
    furi_assert(context);
    Led* instance = context;
    if(!instance->led_state.backlight_always_on) {
        led_backlight_enable(instance, false);
    }
}

static void led_backlight_input_key_callback(const void* value, void* context) {
    furi_assert(context);
    Led* instance = context;

    InputEvent* input_key_event = (InputEvent*)value;
    const LedMessage msg = {.type = LedMessageTypeBacklightInputToggle};
    led_send_message(instance, &msg);
}

static void led_backlight_input_touch_callback(const void* value, void* context) {
    furi_assert(context);
    Led* instance = context;

    InputTouchEvent* input_touch_event = (InputTouchEvent*)value;
    const LedMessage msg = {.type = LedMessageTypeBacklightInputToggle};
    led_send_message(instance, &msg);
}

static Led* led_alloc(void) {
    Led* instance = (Led*)malloc(sizeof(Led));
    memset(instance, 0x00, sizeof(*instance));

    // Initialize backlight
    furi_bsp_display_backlight_init();

    // BSP LED init
    instance->bsp_led = furi_bsp_led_alloc();

    instance->event_loop = furi_event_loop_alloc();
    instance->message_queue = furi_message_queue_alloc(LED_MAX_MESSAGES, sizeof(LedMessage));
    instance->backlight_timer = furi_event_loop_timer_alloc(instance->event_loop, led_backlight_timer_callback, FuriEventLoopTimerTypeOnce, instance);

    instance->led_state.backlight_time = furi_state_alloc(sizeof(uint32_t));
    uint32_t backlight_time_value = 0;
    FuriHalNvmStorage res = furi_hal_nvm_get_uint32(SETTINGS_LED_BACKLIGHT_TIME, &backlight_time_value);
    if((res != FuriHalNvmStorageOK) || (backlight_time_value > LED_BACKLIGHT_TIME_MAX)) {
        backlight_time_value = LED_BACKLIGHT_TIME_DEFAULT;
        FURI_LOG_E(TAG, "Failed to read %s from NVM, defaulting to %lu", SETTINGS_LED_BACKLIGHT_TIME, (unsigned long)backlight_time_value);
        furi_hal_nvm_set_uint32(SETTINGS_LED_BACKLIGHT_TIME, backlight_time_value);
    }
    furi_state_set(instance->led_state.backlight_time, &backlight_time_value);

    for(size_t i = 0; i < LedGroupMax; i++) {
        instance->led_state.brightness[i] = furi_state_alloc(sizeof(uint8_t));
        const char* state_name = led_settings[i].name;

        uint32_t brightness_value = 0;
        if(furi_hal_nvm_get_uint32(state_name, &brightness_value) != FuriHalNvmStorageOK) {
            brightness_value = led_settings[i].default_value;
            FURI_LOG_E(TAG, "Failed to read %s from NVM, defaulting to %lu", state_name, (unsigned long)brightness_value);
            furi_hal_nvm_set_uint32(state_name, brightness_value);
        }

        uint8_t brightness_state = (uint8_t)brightness_value;
        led_process_set_brightness(instance, (LedGroup)i, brightness_state, false);
    }

    furi_event_loop_subscribe_message_queue(instance->event_loop, instance->message_queue, FuriEventLoopEventIn, led_message_queue_callback, instance);

    furi_record_create(RECORD_LEDS, instance);

    furi_pubsub_subscribe(furi_record_open(RECORD_INPUT_EVENTS), led_backlight_input_key_callback, instance);
    furi_pubsub_subscribe(furi_record_open(RECORD_INPUT_TOUCH_EVENTS), led_backlight_input_touch_callback, instance);

    return instance;
}

int32_t led_srv(void* p) {
    UNUSED(p);

    Led* instance = led_alloc();
    furi_event_loop_run(instance->event_loop);

    return 0;
}

void led_set_color_single(Led* instance, FuriBspLedType type, LedColor color) {
    furi_check(instance);

    const LedMessage msg = {
        .type = LedMessageTypeSetColorSingle,
        .set_color_single = {.type = type, .color = color},
    };
    led_send_message(instance, &msg);
}

void led_set_color_batch(Led* instance, const LedBatch* batch) {
    furi_check(instance);

    const LedMessage msg = {
        .type = LedMessageTypeSetColorBatch,
        .set_color_batch = {.items = batch},
    };
    led_send_message(instance, &msg);
}

void led_set_brightness(Led* instance, LedGroup group, uint8_t brightness) {
    furi_check(instance);

    const LedMessage msg = {
        .type = LedMessageTypeSetBrightness,
        .set_brightness = {.group = group, .brightness = brightness},
    };
    led_send_message(instance, &msg);
}

bool led_backlight_set_time(Led* instance, uint32_t timeout_ms) {
    furi_check(instance);
    if(timeout_ms > LED_BACKLIGHT_TIME_MAX) {
        return false;
    }

    const LedMessage msg = {
        .type = LedMessageTypeSetBacklightTime,
        .set_backlight_time = {.timeout_ms = timeout_ms},
    };
    led_send_message(instance, &msg);
    return true;
}

void led_backlight_timeout_control(Led* instance, bool ping, bool set_always_on) {
    furi_check(instance);
    const LedMessage msg = {
        .type = LedMessageTypeBacklightControl,
        .backlight_time_control =
            {
                .flags = (ping ? LedBacklightTimeFlagPing : 0) | (set_always_on ? LedBacklightTimeFlagAlwaysOn : 0),
            },
    };
    led_send_message(instance, &msg);
}

FuriState* led_get_brightness_state(Led* instance, LedGroup group) {
    furi_check(instance);
    furi_check(group < LedGroupMax);
    return instance->led_state.brightness[group];
}

FuriState* led_get_backlight_time_state(Led* instance) {
    furi_check(instance);
    return instance->led_state.backlight_time;
}
