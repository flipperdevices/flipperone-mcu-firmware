#include <furi.h>
#include <gui/gui.h>
#include <headphones/headphones.h>
#include <i2c_intercom/i2c_intercom.h>
#include <i2c_intercom/i2c_registers.h>
#include <i2c_intercom/i2c_registers_map.h>
#include <led/led.h>
#include <haptic/haptic.h>
#include <drivers/drv2605l/drv2605l.h>

#define TAG "I2CNegotiator"

#define I2C_NEGOTIATOR_QUEUE_SIZE 32

typedef struct {
    Gui* gui;
    FuriEventLoop* event_loop;
    FuriMessageQueue* negotiator_queue;
    I2CIntercom* intercom;
    Led* led;
    Haptic* haptic;
} I2CNegotiator;

typedef void (*I2CNegotiatorMessageFunction)(I2CNegotiator* instance, uint16_t value);

typedef struct {
    I2CNegotiatorMessageFunction fn;
    uint16_t value;
} I2CNegotiatorI2CMessage;

#define I2C_NEGOTIATOR_REGISTER_MESSAGE_FROM_IRQ(func)                                           \
    void func##_message(void* context, uint16_t address, uint16_t value) {                       \
        furi_check(context);                                                                     \
        FuriMessageQueue* queue = context;                                                       \
        I2CNegotiatorI2CMessage message = {.fn = func, .value = value};                          \
        FuriStatus stat = furi_message_queue_put(queue, &message, 0);                            \
        furi_check(stat != FuriStatusErrorParameter);                                            \
        if(stat != FuriStatusOk) FURI_LOG_E(TAG, "Failed to receive 0x%04X: %d", address, stat); \
    }

// Input event
static bool i2c_negotiator_input_event_glue(InputEvent* event, void* context) {
    UNUSED(context);
    furi_check(event);
    if(event->type == InputTypePress) {
        with_i2c_register({
            if(i2c_register_update(I2C_BUTTONS_STATE_REG_ADDRESS, event->key, event->key)) {
                // issue interrupt if button state is changed
                i2c_register_set_interrupt(I2C_INPUT_INTERRUPT_REG_ADDRESS, 1 << I2C_INPUT_INTERRUPT_REG_BIT_BUTTONS);
            }
        });
    } else if(event->type == InputTypeRelease) {
        with_i2c_register({
            if(i2c_register_update(I2C_BUTTONS_STATE_REG_ADDRESS, 0, event->key)) {
                // issue interrupt if button state is changed
                i2c_register_set_interrupt(I2C_INPUT_INTERRUPT_REG_ADDRESS, 1 << I2C_INPUT_INTERRUPT_REG_BIT_BUTTONS);
            }
        });
    }

    // we dont consume the event in any case
    return false;
}

// Touch event
static bool i2c_negotiator_input_touch_event_glue(InputTouchEvent* event, void* context) {
    UNUSED(context);
    furi_check(event);
    if(event->type == InputTouchTypeStart || event->type == InputTouchTypeMove || event->type == InputTouchTypeEnd) {
        with_i2c_register({
            bool interrupt = false;

            if(event->type == InputTouchTypeStart || event->type == InputTouchTypeMove) {
                if(i2c_register_update(I2C_TOUCHPAD_X_REG_ADDRESS, event->x, 0xFFFF)) {
                    interrupt = true;
                }

                if(i2c_register_update(I2C_TOUCHPAD_Y_REG_ADDRESS, event->y, 0xFFFF)) {
                    interrupt = true;
                }
            }

            if(i2c_register_update(I2C_TOUCHPAD_PRESS_REG_ADDRESS, event->pressure, 0xFFFF)) {
                interrupt = true;
            }

            if(interrupt) {
                i2c_register_set_interrupt(I2C_INPUT_INTERRUPT_REG_ADDRESS, 1 << I2C_INPUT_INTERRUPT_REG_BIT_TOUCHPAD);
            }
        });
    }

    // we dont consume the event in any case
    return false;
}

// Sw button event
bool i2c_negotiator_input_sw_button_event(SwInputKey key, bool pressed, void* context) {
    UNUSED(context);
    furi_check(key & SwKeyMask);
    FURI_LOG_I(TAG, "SW button event: key=0x%04X pressed=%d", key, pressed);

    with_i2c_register({
        if(pressed) {
            if(i2c_register_update(I2C_SW_BUTTONS_STATE_REG_ADDRESS, key, key)) {
                // issue interrupt if button state is changed
                i2c_register_set_interrupt(I2C_INPUT_INTERRUPT_REG_ADDRESS, 1 << I2C_INPUT_INTERRUPT_REG_BIT_SW_BUTTONS);
            }
        } else {
            if(i2c_register_update(I2C_SW_BUTTONS_STATE_REG_ADDRESS, 0, key)) {
                // issue interrupt if button state is changed
                i2c_register_set_interrupt(I2C_INPUT_INTERRUPT_REG_ADDRESS, 1 << I2C_INPUT_INTERRUPT_REG_BIT_SW_BUTTONS);
            }
        }
    });

    // we dont consume the event in any case
    return false;
}

//Cpu state register
void i2c_negotiator_cpu_state(I2CNegotiator* instance, uint16_t value) {
    FURI_LOG_I(TAG, "CPU state register write: 0x%04X", value);
}
I2C_NEGOTIATOR_REGISTER_MESSAGE_FROM_IRQ(i2c_negotiator_cpu_state);

// Headphones event
static void i2c_negotiator_headphones_event_glue(const void* value, void* ctx) {
    UNUSED(ctx);
    furi_check(value);
    HeadphonesEvent* event = (HeadphonesEvent*)value;
    with_i2c_register({
        i2c_register_update(I2C_HEADPHONES_STATE_REG_ADDRESS, event->hp_status, 0xFFFF);
        i2c_register_set_interrupt(I2C_INPUT_INTERRUPT_REG_ADDRESS, 1 << I2C_INPUT_INTERRUPT_REG_BIT_HEADPHONES);
    });
}

// Led functions
void i2c_negotiator_link_led_brightness_set(I2CNegotiator* instance, uint16_t value) {
    led_set_brightness(instance->led, LedGroupLink, value & 0xFF);
}
I2C_NEGOTIATOR_REGISTER_MESSAGE_FROM_IRQ(i2c_negotiator_link_led_brightness_set);

void i2c_negotiator_power_led_brightness_set(I2CNegotiator* instance, uint16_t value) {
    led_set_brightness(instance->led, LedGroupPower, value & 0xFF);
}
I2C_NEGOTIATOR_REGISTER_MESSAGE_FROM_IRQ(i2c_negotiator_power_led_brightness_set);

void i2c_negotiator_wattmeter_led_brightness_set(I2CNegotiator* instance, uint16_t value) {
    led_set_brightness(instance->led, LedGroupWattmeter, value & 0xFF);
}
I2C_NEGOTIATOR_REGISTER_MESSAGE_FROM_IRQ(i2c_negotiator_wattmeter_led_brightness_set);

void i2c_negotiator_led_link1(I2CNegotiator* instance, uint16_t value) {
    LedColor color = LED_COLOR_RGB565(value);
    led_set_color_single(instance->led, LedTypeNet, color);
}
I2C_NEGOTIATOR_REGISTER_MESSAGE_FROM_IRQ(i2c_negotiator_led_link1);

void i2c_negotiator_led_link2(I2CNegotiator* instance, uint16_t value) {
    LedColor color = LED_COLOR_RGB565(value);
    led_set_color_single(instance->led, LedTypeWiFi, color);
}
I2C_NEGOTIATOR_REGISTER_MESSAGE_FROM_IRQ(i2c_negotiator_led_link2);

void i2c_negotiator_led_link3(I2CNegotiator* instance, uint16_t value) {
    LedColor color = LED_COLOR_RGB565(value);
    led_set_color_single(instance->led, LedTypeEth2, color);
}
I2C_NEGOTIATOR_REGISTER_MESSAGE_FROM_IRQ(i2c_negotiator_led_link3);

void i2c_negotiator_led_link4(I2CNegotiator* instance, uint16_t value) {
    LedColor color = LED_COLOR_RGB565(value);
    led_set_color_single(instance->led, LedTypeEth1, color);
}
I2C_NEGOTIATOR_REGISTER_MESSAGE_FROM_IRQ(i2c_negotiator_led_link4);

// Haptic functions
void i2c_negotiator_haptic_play_effect(I2CNegotiator* instance, uint16_t value) {
    Drv2605lEffect effect_id = (Drv2605lEffect)((value & I2C_HAPTIC_NUM_EFFECT_MASK) >> I2C_HAPTIC_NUM_EFFECT_SHIFT);

    if(effect_id >= Drv2605lEffectCountMax) {
        FURI_LOG_E(TAG, "Invalid haptic effect ID: %d", effect_id);
        return;
    }

    if(value & (1 << I2C_HAPTIC_PLAY_EFFECT_BIT)) {
        uint32_t time_ms = (value & I2C_HAPTIC_DURATION_MASK) >> I2C_HAPTIC_DURATION_SHIFT;
        haptic_play_effect(instance->haptic, effect_id, time_ms <= 1 ? 0 : time_ms);
    } else {
        haptic_stop(instance->haptic);
    }
}
I2C_NEGOTIATOR_REGISTER_MESSAGE_FROM_IRQ(i2c_negotiator_haptic_play_effect);

// Internal functions
static void i2c_negotiator_queue_worker(FuriEventLoopObject* object, void* context) {
    furi_check(context);
    I2CNegotiator* instance = context;
    I2CNegotiatorI2CMessage message;
    while(furi_message_queue_get(object, &message, 0) == FuriStatusOk) {
        furi_check(message.fn);
        message.fn(instance, message.value);
    }
}

static void i2c_negotiator_link_led_brightness_callback(const void* item, void* context) {
    UNUSED(context);
    uint8_t* brightness = (uint8_t*)item;
    with_i2c_register({ i2c_register_update(I2C_LED_BRIGHTNESS_LINK_REG_ADDRESS, *brightness, 0xFF); });
}

static void i2c_negotiator_power_led_brightness_callback(const void* item, void* context) {
    UNUSED(context);
    uint8_t* brightness = (uint8_t*)item;
    with_i2c_register({ i2c_register_update(I2C_LED_BRIGHTNESS_POWER_REG_ADDRESS, *brightness, 0xFF); });
}

static void i2c_negotiator_wattmeter_led_brightness_callback(const void* item, void* context) {
    UNUSED(context);
    uint8_t* brightness = (uint8_t*)item;
    with_i2c_register({ i2c_register_update(I2C_LED_BRIGHTNESS_WATTMETER_REG_ADDRESS, *brightness, 0xFF); });
}

I2CNegotiator* i2c_negotiator_alloc() {
    I2CNegotiator* instance = malloc(sizeof(I2CNegotiator));
    instance->gui = furi_record_open(RECORD_GUI);
    instance->intercom = furi_record_open(RECORD_I2C_INTERCOM);
    instance->led = furi_record_open(RECORD_LEDS);
    instance->haptic = furi_record_open(RECORD_HAPTIC);
    instance->event_loop = furi_event_loop_alloc();

    instance->negotiator_queue = furi_message_queue_alloc(I2C_NEGOTIATOR_QUEUE_SIZE, sizeof(I2CNegotiatorI2CMessage));

    {
        // Version
        i2c_register_add_readable(I2C_INTERCOM_VERSION_REG_ADDRESS, I2C_INTERCOM_VERSION);

        // Input
        // Interrupt register
        i2c_register_add_interrupt(I2C_INPUT_INTERRUPT_REG_ADDRESS, I2C_INPUT_INTERRUPT_MASK_REG_ADDRESS, I2C_STATUS_REG_BIT_INPUT);

        //Cpu state register
        i2c_register_add_writable(I2C_CPU_STATUS_REG_ADDRESS, 0, i2c_negotiator_cpu_state_message, instance->negotiator_queue);

        // Buttons state
        i2c_register_add_readable(I2C_BUTTONS_STATE_REG_ADDRESS, 0);

        // Touchpad state
        i2c_register_add_readable(I2C_TOUCHPAD_X_REG_ADDRESS, 0);
        i2c_register_add_readable(I2C_TOUCHPAD_Y_REG_ADDRESS, 0);
        i2c_register_add_readable(I2C_TOUCHPAD_PRESS_REG_ADDRESS, 0);

        // SW buttons state
        i2c_register_add_readable(I2C_SW_BUTTONS_STATE_REG_ADDRESS, 0);

        // Headphones
        i2c_register_add_readable(I2C_HEADPHONES_STATE_REG_ADDRESS, 0);
        furi_pubsub_subscribe(furi_record_open(RECORD_HEADPHONES), i2c_negotiator_headphones_event_glue, NULL);

        // LEDs
        i2c_register_add_writable(I2C_LED_LINK1_COLOR_REG_ADDRESS, 0, i2c_negotiator_led_link1_message, instance->negotiator_queue);
        i2c_register_add_writable(I2C_LED_LINK2_COLOR_REG_ADDRESS, 0, i2c_negotiator_led_link2_message, instance->negotiator_queue);
        i2c_register_add_writable(I2C_LED_LINK3_COLOR_REG_ADDRESS, 0, i2c_negotiator_led_link3_message, instance->negotiator_queue);
        i2c_register_add_writable(I2C_LED_LINK4_COLOR_REG_ADDRESS, 0, i2c_negotiator_led_link4_message, instance->negotiator_queue);

        i2c_register_add_writable(I2C_LED_BRIGHTNESS_LINK_REG_ADDRESS, 0, i2c_negotiator_link_led_brightness_set_message, instance->negotiator_queue);
        i2c_register_add_writable(I2C_LED_BRIGHTNESS_POWER_REG_ADDRESS, 0, i2c_negotiator_power_led_brightness_set_message, instance->negotiator_queue);
        i2c_register_add_writable(I2C_LED_BRIGHTNESS_WATTMETER_REG_ADDRESS, 0, i2c_negotiator_wattmeter_led_brightness_set_message, instance->negotiator_queue);

        furi_state_subscribe(led_get_brightness_state(instance->led, LedGroupLink), i2c_negotiator_link_led_brightness_callback, NULL);
        furi_state_subscribe(led_get_brightness_state(instance->led, LedGroupPower), i2c_negotiator_power_led_brightness_callback, NULL);
        furi_state_subscribe(led_get_brightness_state(instance->led, LedGroupWattmeter), i2c_negotiator_wattmeter_led_brightness_callback, NULL);
        // TODO: backlight

        // Haptic
        i2c_register_add_writable(I2C_HAPTIC_PLAY_EFFECT_REG_ADDRESS, 0, i2c_negotiator_haptic_play_effect_message, instance->negotiator_queue);
    }

    furi_event_loop_subscribe_message_queue(instance->event_loop, instance->negotiator_queue, FuriEventLoopEventIn, i2c_negotiator_queue_worker, instance);

    gui_add_unhandled_input_callback(instance->gui, i2c_negotiator_input_event_glue, instance);
    gui_add_unhandled_touch_input_callback(instance->gui, i2c_negotiator_input_touch_event_glue, instance);

    i2c_intercom_setup_end(instance->intercom);

    return instance;
}

int32_t i2c_negotiator_srv(void* p) {
    UNUSED(p);

    I2CNegotiator* instance = i2c_negotiator_alloc();

    furi_event_loop_run(instance->event_loop);

    return 0;
}
