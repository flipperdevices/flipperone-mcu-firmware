#include <furi.h>
#include <gui/gui.h>
#include <headphones/headphones.h>
#include <i2c_intercom/i2c_intercom.h>
#include <i2c_intercom/i2c_registers.h>
#include <i2c_intercom/i2c_registers_map.h>

#define TAG "I2CNegotiator"

typedef struct {
    Gui* gui;
    View* view;
    FuriEventLoop* event_loop;
    I2cIntercom* intercom;
} I2CNegotiator;

static bool i2c_negotiator_input_event_glue(InputEvent* event, void* context) {
    UNUSED(context);
    furi_check(event);
    if(event->type == InputTypePress) {
        with_i2c_register({
            i2c_register_update(I2C_BUTTONS_STATE_REG_ADDRESS, event->key, event->key);
            i2c_register_set_interrupt(I2C_INPUT_INTERRUPT_REG_ADDRESS, 1 << I2C_INPUT_INTERRUPT_REG_BIT_BUTTONS);
        });
    } else if(event->type == InputTypeRelease) {
        with_i2c_register({
            i2c_register_update(I2C_BUTTONS_STATE_REG_ADDRESS, 0, event->key);
            i2c_register_set_interrupt(I2C_INPUT_INTERRUPT_REG_ADDRESS, 1 << I2C_INPUT_INTERRUPT_REG_BIT_BUTTONS);
        });
    }

    // we dont consume the event in any case
    return false;
}

static bool i2c_negotiator_input_touch_event_glue(InputTouchEvent* event, void* context) {
    UNUSED(context);
    furi_check(event);
    if(event->type == InputTouchTypeStart || event->type == InputTouchTypeMove || event->type == InputTouchTypeEnd) {
        with_i2c_register({
            if(event->type == InputTouchTypeStart || event->type == InputTouchTypeMove) {
                i2c_register_update(I2C_TOUCHPAD_X_REG_ADDRESS, event->x, 0xFFFF);
                i2c_register_update(I2C_TOUCHPAD_Y_REG_ADDRESS, event->y, 0xFFFF);
            }
            i2c_register_update(I2C_TOUCHPAD_PRESS_REG_ADDRESS, event->pressure, 0xFFFF);
            i2c_register_set_interrupt(I2C_INPUT_INTERRUPT_REG_ADDRESS, 1 << I2C_INPUT_INTERRUPT_REG_BIT_TOUCHPAD);
        });
    }

    // we dont consume the event in any case
    return false;
}

static void i2c_negotiator_headphones_event_glue(const void* value, void* ctx) {
    UNUSED(ctx);
    furi_check(value);
    HeadphonesEvent* event = (HeadphonesEvent*)value;
    with_i2c_register({
        i2c_register_update(I2C_HEADPHONES_STATE_REG_ADDRESS, event->hp_status, 0xFFFF);
        i2c_register_set_interrupt(I2C_INPUT_INTERRUPT_REG_ADDRESS, 1 << I2C_INPUT_INTERRUPT_REG_BIT_HEADPHONES);
    });
}

void i2c_negotiator_test_callback(void* context, uint16_t value) {
    FURI_LOG_I(TAG, "Test callback called with value: %x, ctx: %p", value, context);
}

I2CNegotiator* i2c_negotiator_alloc() {
    I2CNegotiator* instance = malloc(sizeof(I2CNegotiator));
    instance->gui = furi_record_open(RECORD_GUI);
    instance->intercom = furi_record_open(RECORD_I2C_INTERCOM);
    instance->event_loop = furi_event_loop_alloc();

    instance->view = view_alloc();
    view_set_transparent(instance->view, true);

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
        i2c_register_add_writable(0x0300, 0, i2c_negotiator_test_callback, (void*)0xDEADBEEF);
    }

    view_set_input_callback(instance->view, i2c_negotiator_input_event_glue, instance);
    view_set_input_touch_callback(instance->view, i2c_negotiator_input_touch_event_glue, instance);
    gui_add_view(instance->gui, instance->view, GuiViewPriorityIntercom);

    i2c_intercom_setup_end(instance->intercom);

    return instance;
}

int32_t i2c_negotiator_srv(void* p) {
    UNUSED(p);

    I2CNegotiator* instance = i2c_negotiator_alloc();

    furi_event_loop_run(instance->event_loop);

    return 0;
}
