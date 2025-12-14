#include <input/input.h>

#include <furi.h>
#include <furi_hal_resources.h>

#define INPUT_DEBOUNCE_TICKS      4
#define INPUT_DEBOUNCE_TICKS_HALF (INPUT_DEBOUNCE_TICKS / 2)
#define INPUT_PRESS_TICKS         150
#define INPUT_LONG_PRESS_COUNTS   2
#define INPUT_THREAD_FLAG_ISR     0x00000001

#define TAG "InputTestSrv"

typedef struct {
    const InputPin* pin;
    const GpioPin* gpio;
} InputPinGpio;

const InputPinGpio input_pins_gpio[] = {
    {.pin = &input_pins[0], .gpio = &gpio_key1},
    {.pin = &input_pins[1], .gpio = &gpio_key2},
    {.pin = &input_pins[2], .gpio = &gpio_key3},
    {.pin = &input_pins[3], .gpio = &gpio_key4},
    {.pin = &input_pins[4], .gpio = &gpio_key5},
    {.pin = &input_pins[5], .gpio = &gpio_key_back},
    {.pin = &input_pins[6], .gpio = &gpio_key_up},
    {.pin = &input_pins[7], .gpio = &gpio_key_down},
    {.pin = &input_pins[8], .gpio = &gpio_key_right},
    {.pin = &input_pins[9], .gpio = &gpio_key_left},
    {.pin = &input_pins[10], .gpio = &gpio_key_center},
    {.pin = &input_pins[11], .gpio = NULL},
    {.pin = &input_pins[12], .gpio = &gpio_key_sw},
};

/** Input pin state */
typedef struct {
    const InputPinGpio* gpio;
    // State
    volatile bool state;
    volatile uint8_t debounce;
    FuriTimer* press_timer;
    FuriPubSub* event_pubsub;
    volatile uint8_t press_counter;
    volatile uint32_t counter;
} InputPinState;

#define GPIO_Read(input_pin) (furi_hal_gpio_read(input_pin.gpio->gpio) ^ (input_pin.gpio->pin->inverted))

static void input_press_timer_callback(void* arg) {
    InputPinState* input_pin = arg;
    InputEvent event;
    event.sequence_source = INPUT_SEQUENCE_SOURCE_HARDWARE;
    event.sequence_counter = input_pin->counter;
    event.key = input_pin->gpio->pin->key;
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

static void input_isr(void* _ctx) {
    FuriThreadId thread_id = (FuriThreadId)_ctx;
    furi_thread_flags_set(thread_id, INPUT_THREAD_FLAG_ISR);
}

int32_t input_test_srv(void* p) {
    UNUSED(p);

    furi_check(COUNT_OF(input_pins_gpio) == input_pins_count);

    const FuriThreadId thread_id = furi_thread_get_current_id();
    FuriPubSub* event_pubsub = furi_pubsub_alloc();
    uint32_t counter = 1;
    furi_record_create(RECORD_INPUT_EVENTS, event_pubsub);

#ifdef INPUT_DEBUG
    furi_hal_gpio_init_simple(&gpio_ext_pa4, GpioModeOutputPushPull);
#endif

#ifdef SRV_CLI
    CliRegistry* registry = furi_record_open(RECORD_CLI);
    cli_registry_add_command(registry, "input", CliCommandFlagParallelSafe, input_cli, event_pubsub);
    furi_record_close(RECORD_CLI);
#endif

    InputPinState pin_states[input_pins_count];

    for(size_t i = 0; i < input_pins_count; i++) {
        if(input_pins_gpio[i].gpio == NULL) continue;

        furi_hal_gpio_init_simple(input_pins_gpio[i].gpio, GpioModeInput);
        furi_hal_gpio_add_int_callback(input_pins_gpio[i].gpio, GpioConditionRiseFall, input_isr, thread_id);

        pin_states[i].gpio = &input_pins_gpio[i];
        pin_states[i].state = GPIO_Read(pin_states[i]);
        pin_states[i].debounce = INPUT_DEBOUNCE_TICKS_HALF;
        pin_states[i].press_timer = furi_timer_alloc(input_press_timer_callback, FuriTimerTypePeriodic, &pin_states[i]);
        pin_states[i].event_pubsub = event_pubsub;
        pin_states[i].press_counter = 0;
    }

    while(1) {
        bool is_changing = false;
        for(size_t i = 0; i < input_pins_count; i++) {
            if(input_pins_gpio[i].gpio == NULL) continue;

            bool state = GPIO_Read(pin_states[i]);
            if(state) {
                if(pin_states[i].debounce < INPUT_DEBOUNCE_TICKS) pin_states[i].debounce += 1;
            } else {
                if(pin_states[i].debounce > 0) pin_states[i].debounce -= 1;
            }

            if(pin_states[i].debounce > 0 && pin_states[i].debounce < INPUT_DEBOUNCE_TICKS) {
                is_changing = true;
            } else if(pin_states[i].state != state) {
                pin_states[i].state = state;

                // Common state info
                InputEvent event;
                event.sequence_source = INPUT_SEQUENCE_SOURCE_HARDWARE;
                event.key = pin_states[i].gpio->pin->key;

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

        if(is_changing) {
#ifdef INPUT_DEBUG
            furi_hal_gpio_write(&gpio_ext_pa4, 1);
#endif
            furi_delay_tick(1);
        } else {
#ifdef INPUT_DEBUG
            furi_hal_gpio_write(&gpio_ext_pa4, 0);
#endif
            furi_thread_flags_wait(INPUT_THREAD_FLAG_ISR, FuriFlagWaitAny, FuriWaitForever);
        }
    }

    return 0;
}
