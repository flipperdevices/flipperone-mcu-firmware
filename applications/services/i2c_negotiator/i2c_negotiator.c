#include <furi.h>
#include <gui/gui.h>
#include <headphones/headphones.h>
#include <i2c_intercom/i2c_intercom.h>
#include <i2c_intercom/i2c_registers.h>
#include <i2c_intercom/i2c_registers_map.h>
#include <led/led.h>

#define TAG "I2CNegotiator"

#define I2C_NEGOTIATOR_LED_QUEUE_SIZE 16

typedef struct {
    Gui* gui;
    FuriEventLoop* event_loop;
    FuriMessageQueue* led_queue;
    I2CIntercom* intercom;
    Led* led;
} I2CNegotiator;

typedef void (*I2CNegotiatorMessageFunction)(I2CNegotiator* instance, uint16_t value);

typedef struct {
    I2CNegotiatorMessageFunction fn;
    uint16_t value;
} I2CNegotiatorI2CMessage;

#define I2C_NEGOTIATOR_REGISTER_MESSAGE(func, timeout)                                            \
    void func##_message(void* context, uint16_t address, uint16_t value) {                        \
        UNUSED(address);                                                                          \
        furi_check(context);                                                                      \
        FuriMessageQueue* queue = context;                                                        \
        I2CNegotiatorI2CMessage message = {.fn = func, .value = value};                           \
        furi_check(furi_message_queue_put(queue, &message, timeout) != FuriStatusErrorParameter); \
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
void i2c_negotiator_led_link1(I2CNegotiator* instance, uint16_t value) {
    LedColor color = LED_COLOR_RGB565(value);
    led_set_color_single(instance->led, LedTypeNet, color);
}
I2C_NEGOTIATOR_REGISTER_MESSAGE(i2c_negotiator_led_link1, 0);

void i2c_negotiator_led_link2(I2CNegotiator* instance, uint16_t value) {
    LedColor color = LED_COLOR_RGB565(value);
    led_set_color_single(instance->led, LedTypeWiFi, color);
}
I2C_NEGOTIATOR_REGISTER_MESSAGE(i2c_negotiator_led_link2, 0);

void i2c_negotiator_led_link3(I2CNegotiator* instance, uint16_t value) {
    LedColor color = LED_COLOR_RGB565(value);
    led_set_color_single(instance->led, LedTypeEth2, color);
}
I2C_NEGOTIATOR_REGISTER_MESSAGE(i2c_negotiator_led_link3, 0);

void i2c_negotiator_led_link4(I2CNegotiator* instance, uint16_t value) {
    LedColor color = LED_COLOR_RGB565(value);
    led_set_color_single(instance->led, LedTypeEth1, color);
}
I2C_NEGOTIATOR_REGISTER_MESSAGE(i2c_negotiator_led_link4, 0);

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

I2CNegotiator* i2c_negotiator_alloc() {
    I2CNegotiator* instance = malloc(sizeof(I2CNegotiator));
    instance->gui = furi_record_open(RECORD_GUI);
    instance->intercom = furi_record_open(RECORD_I2C_INTERCOM);
    instance->led = furi_record_open(RECORD_LEDS);
    instance->event_loop = furi_event_loop_alloc();

    instance->led_queue = furi_message_queue_alloc(I2C_NEGOTIATOR_LED_QUEUE_SIZE, sizeof(I2CNegotiatorI2CMessage));

    {
        // Version
        i2c_register_add_readable(I2C_INTERCOM_VERSION_REG_ADDRESS, I2C_INTERCOM_VERSION);

        // Input
        // Interrupt register
        i2c_register_add_interrupt(I2C_INPUT_INTERRUPT_REG_ADDRESS, I2C_INPUT_INTERRUPT_MASK_REG_ADDRESS, I2C_STATUS_REG_BIT_INPUT);

        // Buttons state
        i2c_register_add_readable(I2C_BUTTONS_STATE_REG_ADDRESS, 0);

        // Touchpad state
        i2c_register_add_readable(I2C_TOUCHPAD_X_REG_ADDRESS, 0);
        i2c_register_add_readable(I2C_TOUCHPAD_Y_REG_ADDRESS, 0);
        i2c_register_add_readable(I2C_TOUCHPAD_PRESS_REG_ADDRESS, 0);

        // Headphones
        i2c_register_add_readable(I2C_HEADPHONES_STATE_REG_ADDRESS, 0);
        furi_pubsub_subscribe(furi_record_open(RECORD_HEADPHONES), i2c_negotiator_headphones_event_glue, NULL);

        // Test writable register
        // `sudo i2ctransfer -y 0 w4@0x69 0x03 0x00 0x34 0x12` will write 0x1234 to address 0x0300
        i2c_register_add_writable(I2C_LED_LINK1_COLOR_REG_ADDRESS, 0, i2c_negotiator_led_link1_message, instance->led_queue);
        i2c_register_add_writable(I2C_LED_LINK2_COLOR_REG_ADDRESS, 0, i2c_negotiator_led_link2_message, instance->led_queue);
        i2c_register_add_writable(I2C_LED_LINK3_COLOR_REG_ADDRESS, 0, i2c_negotiator_led_link3_message, instance->led_queue);
        i2c_register_add_writable(I2C_LED_LINK4_COLOR_REG_ADDRESS, 0, i2c_negotiator_led_link4_message, instance->led_queue);
    }

    furi_event_loop_subscribe_message_queue(instance->event_loop, instance->led_queue, FuriEventLoopEventIn, i2c_negotiator_queue_worker, instance);

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
