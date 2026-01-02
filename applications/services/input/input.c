#include "input.h"

#include <furi_hal_i2c_config.h>
#include <furi.h>
#include <drivers/tca6416a/tca6416a.h>
#include <drivers/drv2605l/drv2605l.h>

#define INPUT_DEBOUNCE_TICKS      4
#define INPUT_DEBOUNCE_TICKS_HALF (INPUT_DEBOUNCE_TICKS / 2)
#define INPUT_PRESS_TICKS         150
#define INPUT_LONG_PRESS_COUNTS   2
#define INPUT_THREAD_FLAG_ISR     0x00000001

/** Input pin state */
typedef struct {
    const InputPin* pin;
    // State
    volatile bool state;
    volatile uint8_t debounce;
    FuriTimer* press_timer;
    FuriPubSub* event_pubsub;
    volatile uint8_t press_counter;
    volatile uint32_t counter;
} InputPinState;

static bool input_key_check_state(uint16_t state_pin, InputPinState input_pin) {
    bool val = (state_pin & input_pin.pin->key) ? true : false;
    if(input_pin.pin->inverted) {
        val = !val;
    }
    return val;
}

void input_press_timer_callback(void* arg) {
    InputPinState* input_pin = arg;
    InputEvent event;
    event.sequence_source = INPUT_SEQUENCE_SOURCE_HARDWARE;
    event.sequence_counter = input_pin->counter;
    event.key = input_pin->pin->key;
    input_pin->press_counter++;
    if(input_pin->press_counter == INPUT_LONG_PRESS_COUNTS) {
        event.type = InputTypeLong;
        furi_pubsub_publish(input_pin->event_pubsub, &event);
    } else if(input_pin->press_counter > INPUT_LONG_PRESS_COUNTS) {
        input_pin->press_counter--;
        event.type = InputTypeRepeat;
        furi_pubsub_publish(input_pin->event_pubsub, &event);
    }
}

void input_isr(void* _ctx) {
    FuriThreadId thread_id = (FuriThreadId)_ctx;
    furi_thread_flags_set(thread_id, INPUT_THREAD_FLAG_ISR);
}

const char* input_get_key_name(InputKey key) {
    for(size_t i = 0; i < input_pins_count; i++) {
        if(input_pins[i].key == key) {
            return input_pins[i].name;
        }
    }
    return "Unknown";
}

const char* input_get_type_name(InputType type) {
    switch(type) {
    case InputTypePress:
        return "Press";
    case InputTypeRelease:
        return "Release";
    case InputTypeShort:
        return "Short";
    case InputTypeLong:
        return "Long";
    case InputTypeRepeat:
        return "Repeat";
    default:
        return "Unknown";
    }
}

int32_t input_srv(void* p) {
    UNUSED(p);

    const FuriThreadId thread_id = furi_thread_get_current_id();
    FuriPubSub* event_pubsub = furi_pubsub_alloc();
    uint32_t counter = 1;
    furi_record_create(RECORD_INPUT_EVENTS, event_pubsub);

#ifdef INPUT_DEBUG
    furi_hal_gpio_init_simple(&debug_pin1, GpioModeOutputPushPull);
#endif

#ifdef SRV_CLI
    CliRegistry* registry = furi_record_open(RECORD_CLI);
    cli_registry_add_command(registry, "input", CliCommandFlagParallelSafe, input_cli, event_pubsub);
    furi_record_close(RECORD_CLI);
#endif

    InputPinState pin_states[input_pins_count];

    Tca6416a* tca6416a = tca6416a_init(&furi_hal_i2c_handle_internal, &gpio_expander_reset, &gpio_expander_int, TCA6416A_ADDRESS_A0);

    tca6416a_write_mode(tca6416a, InputKeyMask);
    tca6416a_write_output(tca6416a, ~InputKeyMask);
    tca6416a_set_input_callback(tca6416a, input_isr, thread_id);

    uint16_t input_state = tca6416a_read_input(tca6416a);

    Drv2605l* drv2605l = drv2605l_init(&furi_hal_i2c_handle_internal, &gpio_haptic_en, &gpio_haptic_pwm, DRV2605L_ADDRESS);

    for(size_t i = 0; i < input_pins_count; i++) {
        pin_states[i].pin = &input_pins[i];
        pin_states[i].state = input_key_check_state(input_state, pin_states[i]);
        pin_states[i].debounce = INPUT_DEBOUNCE_TICKS_HALF;
        pin_states[i].press_timer = furi_timer_alloc(input_press_timer_callback, FuriTimerTypePeriodic, &pin_states[i]);
        pin_states[i].event_pubsub = event_pubsub;
        pin_states[i].press_counter = 0;
    }

    while(1) {
        bool is_changing = false;
        input_state = tca6416a_read_input(tca6416a);

        for(size_t i = 0; i < input_pins_count; i++) {
            bool state = input_key_check_state(input_state, pin_states[i]);
            if(state) {
                if(pin_states[i].debounce < INPUT_DEBOUNCE_TICKS) pin_states[i].debounce += 1;
            } else {
                if(pin_states[i].debounce > 0) pin_states[i].debounce -= 1;
            }

            if(pin_states[i].debounce > 0 && pin_states[i].debounce < INPUT_DEBOUNCE_TICKS) {
                is_changing = true;
            } else if(pin_states[i].state != state) {
                pin_states[i].state = state;

                if(state) {
                    drv2605l_trigger_set_effect_and_play(drv2605l, Drv2605lEffectSoftBump_100);
                }
               
                // Common state info
                InputEvent event;
                event.sequence_source = INPUT_SEQUENCE_SOURCE_HARDWARE;
                event.key = pin_states[i].pin->key;

                // Short / Long / Repeat timer routine
                if(state) {
                    pin_states[i].counter = counter++;
                    event.sequence_counter = pin_states[i].counter;
                    furi_timer_start(pin_states[i].press_timer, INPUT_PRESS_TICKS);
                } else {
                    event.sequence_counter = pin_states[i].counter;
                    furi_timer_stop(pin_states[i].press_timer);
                    while(furi_timer_is_running(pin_states[i].press_timer))
                        furi_delay_tick(1);
                    if(pin_states[i].press_counter < INPUT_LONG_PRESS_COUNTS) {
                        event.type = InputTypeShort;
                        furi_pubsub_publish(event_pubsub, &event);
                    }
                    pin_states[i].press_counter = 0;
                }

                // Send Press/Release event
                event.type = pin_states[i].state ? InputTypePress : InputTypeRelease;
                furi_pubsub_publish(event_pubsub, &event);
            }
        }

        if(furi_hal_gpio_read(&gpio_expander_int) == 0) {
            furi_thread_flags_set(thread_id, INPUT_THREAD_FLAG_ISR);
        }

        if(is_changing) {
#ifdef INPUT_DEBUG
            furi_hal_gpio_write(&debug_pin1, 1);
#endif
            furi_delay_tick(1);
        } else {
#ifdef INPUT_DEBUG
            furi_hal_gpio_write(&debug_pin1, 0);
#endif
            furi_thread_flags_wait(INPUT_THREAD_FLAG_ISR, FuriFlagWaitAny, FuriWaitForever);
        }
    }

    return 0;
}
