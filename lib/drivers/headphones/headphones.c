#include "headphones.h"
#include <furi.h>
#include <furi_hal_adc.h>

#define TAG "Headphones"

//#define HEADPHONES_DEBUG_ENABLE

#ifdef HEADPHONES_DEBUG_ENABLE
#define HEADPHONES_DEBUG(...) FURI_LOG_D(__VA_ARGS__)
#else
#define HEADPHONES_DEBUG(...)
#endif

#define ADC_DIFF(x, y) (((x) < (y)) ? ((y) - (x)) : ((x) - (y)))

/*
Control Function	Accessory Support	Description
Function A	Required	Play/Pause/Intercept (short press), Trigger Help (long press), "Next" (double press)
Function B	Optional	Volume Up
Function C	Optional	Volume Down
Function D	Optional	Reserved (Pixel devices use this for voice command launch)
*/

/*Control Function	Equivalent Impedance*
0 Ohm	                    [Function A] Play/Pause/Hook
240 Ohm +/- 1% resistance	[Function B]
470 Ohm +/- 1% resistance	[Function C]
135 Ohm +/- 1% resistance	[Function D]
*Overall impedance from the positive terminal of the microphone to GND 
when the button is pressed with a 2.2V bias applied through a 2.2 kOhm resistor.*/

#define HEADPHONES_MIN_ADC_VALUE_KEY_PRESSED_A        0 //(0.0f)
#define HEADPHONES_MAX_ADC_VALUE_KEY_PRESSED_A        31 //(0.05f)
#define HEADPHONES_MIN_ADC_VALUE_KEY_PRESSED_B        124 //(0.2f)
#define HEADPHONES_MAX_ADC_VALUE_KEY_PRESSED_B        143 //(0.23f)
#define HEADPHONES_MIN_ADC_VALUE_KEY_PRESSED_C        200 //(0.37f)
#define HEADPHONES_MAX_ADC_VALUE_KEY_PRESSED_C        215 //(0.4f)
#define HEADPHONES_MIN_ADC_VALUE_KEY_PRESSED_D        60 //(0.11f)
#define HEADPHONES_MAX_ADC_VALUE_KEY_PRESSED_D        75 //(0.14f)
#define HEADPHONES_MIN_ADC_VALUE_MICROPHONE_CONNECTED (1 << 12) / 2 //(3.3v/2)

typedef struct {
    HeadphonesStatus state; // Placeholder for actual headphone driver state
    const GpioPin* headphone_detect_pin;
    const GpioPin* headphone_key_pin;
    uint16_t adc_max_value;
    HeadphonesCallback callback;
    void* callback_context;
} Headphones;

static Headphones* headphones_instance = NULL;

void headphones_interrupt_handler(void* ctx) {
    Headphones* instance = (Headphones*)ctx;

    // if(furi_hal_gpio_read(instance->headphone_detect_pin)) {
    //     instance->state |= HeadphonesStatusConnected;
    //     instance->state &= ~HeadphonesStatusDisconnected;
    // } else {
    //     instance->state = HeadphonesStatusDisconnected;
    //     instance->adc_max_value = 0;
    // }

    bool connected = furi_hal_gpio_read(instance->headphone_detect_pin);

    if(instance->callback) {
        instance->callback(instance->callback_context, connected);
    }
}

void headphones_init(const GpioPin* headphone_detect_pin, const GpioPin* headphone_key_pin, HeadphonesCallback callback, void* callback_context) {
    furi_check(headphones_instance == NULL);
    headphones_instance = malloc(sizeof(Headphones));

    headphones_instance->headphone_detect_pin = headphone_detect_pin;
    headphones_instance->headphone_key_pin = headphone_key_pin;
    headphones_instance->callback = callback;
    headphones_instance->callback_context = callback_context;
    headphones_instance->adc_max_value = 0;

    furi_hal_gpio_init_simple(headphones_instance->headphone_detect_pin, GpioModeInput);
    furi_hal_gpio_add_int_callback(headphones_instance->headphone_detect_pin, GpioConditionRiseFall, headphones_interrupt_handler, headphones_instance);

    furi_hal_adc_gpio_init(headphones_instance->headphone_key_pin);

    //check initial state
    if(furi_hal_gpio_read(headphones_instance->headphone_detect_pin)) {
        headphones_instance->state |= HeadphonesStatusConnected;
        if(headphones_instance->callback) {
            headphones_instance->callback(headphones_instance->callback_context, headphones_instance->state);
        }
    } else {
        headphones_instance->state |= HeadphonesStatusDisconnected;
    }

    headphones_instance->state = 0; // Initial state
    FURI_LOG_I(TAG, "Headphones initialized");
}

void headphones_deinit(void) {
    furi_check(headphones_instance);
    furi_hal_gpio_remove_int_callback(headphones_instance->headphone_detect_pin);
    furi_hal_gpio_init_ex(headphones_instance->headphone_detect_pin, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_init_ex(headphones_instance->headphone_key_pin, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    headphones_instance->callback = NULL;
    headphones_instance->callback_context = NULL;
    free(headphones_instance);
    headphones_instance = NULL;
    FURI_LOG_I(TAG, "Headphones deinitialized");
}

bool headphones_update(HeadphonesStatus* status) {
    furi_check(headphones_instance);
    HeadphonesStatus old_state = headphones_instance->state;

    if(furi_hal_gpio_read(headphones_instance->headphone_detect_pin)) {
        headphones_instance->state |= HeadphonesStatusConnected;
        headphones_instance->state &= ~HeadphonesStatusDisconnected;
    } else {
        headphones_instance->state = HeadphonesStatusDisconnected;
        headphones_instance->adc_max_value = 0;
    }

    if(headphones_instance->state & HeadphonesStatusDisconnected) {
        if(old_state != headphones_instance->state) {
            *status = headphones_instance->state;
            return true;
        }
        return false; // No need to check keys if headphones are disconnected
    }

    uint16_t adc_value = furi_hal_adc_read(headphones_instance->headphone_key_pin);

    uint8_t double_check = 4;
    do {
        furi_delay_ms(20);
        uint16_t adc_value_temp = furi_hal_adc_read(headphones_instance->headphone_key_pin);
        if(ADC_DIFF(adc_value, adc_value_temp) < 10) {
            break;
        }
    } while(double_check--);

    if(double_check == 0) {
        HEADPHONES_DEBUG(TAG, "ADC value fluctuating too much, ignoring this reading. Last stable value: %u", adc_value);
        return false;
    }

#ifdef HEADPHONES_DEBUG_ENABLE
    float adc_voltage = furi_hal_adc_read_voltage(headphones_instance->headphone_key_pin);
    HEADPHONES_DEBUG(
        "TAG", "Hp detect %u, ADC Value: %u, Voltage: %.2f V", furi_hal_gpio_read(headphones_instance->headphone_detect_pin), adc_value, adc_voltage);
#endif

    if(adc_value >= HEADPHONES_MIN_ADC_VALUE_KEY_PRESSED_A && adc_value <= HEADPHONES_MAX_ADC_VALUE_KEY_PRESSED_A) {
        headphones_instance->state &= ~HEADPHONES_STATUS_KEY_PRESSED_MASK; // Clear all key pressed states
        headphones_instance->state |= HeadphonesStatusKeyPressedA;
    } else {
        if(adc_value >= HEADPHONES_MIN_ADC_VALUE_KEY_PRESSED_B && adc_value <= HEADPHONES_MAX_ADC_VALUE_KEY_PRESSED_B) {
            headphones_instance->state &= ~HEADPHONES_STATUS_KEY_PRESSED_MASK; // Clear all key pressed states
            headphones_instance->state |= HeadphonesStatusKeyPressedB;
        } else {
            if(adc_value >= HEADPHONES_MIN_ADC_VALUE_KEY_PRESSED_C && adc_value <= HEADPHONES_MAX_ADC_VALUE_KEY_PRESSED_C) {
                headphones_instance->state &= ~HEADPHONES_STATUS_KEY_PRESSED_MASK; // Clear all key pressed states
                headphones_instance->state |= HeadphonesStatusKeyPressedC;
            } else {
                if(adc_value >= HEADPHONES_MIN_ADC_VALUE_KEY_PRESSED_D && adc_value <= HEADPHONES_MAX_ADC_VALUE_KEY_PRESSED_D) {
                    headphones_instance->state &= ~HEADPHONES_STATUS_KEY_PRESSED_MASK; // Clear all key pressed states
                    headphones_instance->state |= HeadphonesStatusKeyPressedD;
                } else {
                    if(adc_value > HEADPHONES_MIN_ADC_VALUE_MICROPHONE_CONNECTED) {
                        headphones_instance->state &= ~HEADPHONES_STATUS_KEY_PRESSED_MASK; // Clear all key pressed states
                        headphones_instance->state |= HeadphonesStatusMicrophoneConnected;
                        if(adc_value > headphones_instance->adc_max_value) {
                            headphones_instance->adc_max_value = adc_value;
                            HEADPHONES_DEBUG("TAG", "New max ADC value observed: %u", headphones_instance->adc_max_value);
                        }
                    } else {
                        headphones_instance->state &= ~HEADPHONES_STATUS_KEY_PRESSED_MASK; // Clear all key pressed states
                    }
                }
            }
        }
    }
    if(old_state != headphones_instance->state) {
        *status = headphones_instance->state;
        return true;
        //headphones_instance->callback(headphones_instance->callback_context, headphones_instance->state);
    }
    return false;
}
