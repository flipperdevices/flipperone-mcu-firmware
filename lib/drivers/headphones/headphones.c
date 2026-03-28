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

// Configuration
#define HEADPHONES_R_PULL_UP 2200 // ohms, pull-up resistor
#define HEADPHONES_MBIAS_V   (1.5f) // V

#define HEADPHONES_MIN_ADC_VALUE_KEY_PRESSED_A 0
#define HEADPHONES_MAX_ADC_VALUE_KEY_PRESSED_A 100
#define HEADPHONES_MIN_ADC_VALUE_KEY_PRESSED_B 200
#define HEADPHONES_MAX_ADC_VALUE_KEY_PRESSED_B 360
#define HEADPHONES_MIN_ADC_VALUE_KEY_PRESSED_C 380
#define HEADPHONES_MAX_ADC_VALUE_KEY_PRESSED_C 800
#define HEADPHONES_MIN_ADC_VALUE_KEY_PRESSED_D 100
#define HEADPHONES_MAX_ADC_VALUE_KEY_PRESSED_D 200

#define HEADPHONES_ADC_VALUE_DELTA 10
#define HEADPHONES_ADC_DEBOUNCE_MS 50

typedef struct {
    HeadphonesStatus state; // Placeholder for actual headphone driver state
    const GpioPin* headphone_detect_pin;
    const GpioPin* headphone_key_pin;
    float max_mic_bias_v;
    HeadphonesCallback callback;
    void* callback_context;
} Headphones;

static Headphones* headphones_instance = NULL;

void headphones_interrupt_handler(void* ctx) {
    Headphones* instance = (Headphones*)ctx;

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
    headphones_instance->max_mic_bias_v = HEADPHONES_MBIAS_V;
    headphones_instance->state = HeadphonesStatusDisconnected;

    furi_hal_gpio_init_simple(headphones_instance->headphone_detect_pin, GpioModeInput);
    furi_hal_gpio_add_int_callback(headphones_instance->headphone_detect_pin, GpioConditionRiseFall, headphones_interrupt_handler, headphones_instance);

    furi_hal_adc_gpio_init(headphones_instance->headphone_key_pin);

    //check initial state
    if(furi_hal_gpio_read(headphones_instance->headphone_detect_pin)) {
        float adc_voltage = furi_hal_adc_read_voltage(headphones_instance->headphone_key_pin);
        if(adc_voltage > headphones_instance->max_mic_bias_v) {
            headphones_instance->max_mic_bias_v = adc_voltage;
            HEADPHONES_DEBUG(TAG, "New max ADC voltage observed: %.4f V", headphones_instance->max_mic_bias_v);
        }
        if(headphones_instance->callback) {
            headphones_instance->callback(headphones_instance->callback_context, headphones_instance->state);
        }
    }

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

static HeadphonesStatus headphones_detect_button(float adc_v) {
    float r_total = HEADPHONES_R_PULL_UP * (adc_v / (headphones_instance->max_mic_bias_v - adc_v));

    HEADPHONES_DEBUG(TAG, "Calculated R_total: %.2f Ohm for ADC: Uin %.2f V, Uout %.2f V", r_total, headphones_instance->max_mic_bias_v, adc_v);

    if(r_total > HEADPHONES_MIN_ADC_VALUE_KEY_PRESSED_A && r_total < HEADPHONES_MAX_ADC_VALUE_KEY_PRESSED_A) {
        return HeadphonesStatusKeyPressedA; // 0 Ohm (Function A)
    } else if(r_total > HEADPHONES_MIN_ADC_VALUE_KEY_PRESSED_D && r_total < HEADPHONES_MAX_ADC_VALUE_KEY_PRESSED_D) {
        return HeadphonesStatusKeyPressedD; // ~135 Ohm (Function D)
    } else if(r_total > HEADPHONES_MIN_ADC_VALUE_KEY_PRESSED_B && r_total < HEADPHONES_MAX_ADC_VALUE_KEY_PRESSED_B) {
        return HeadphonesStatusKeyPressedB; // ~240 Ohm (Function B)
    } else if(r_total > HEADPHONES_MIN_ADC_VALUE_KEY_PRESSED_C && r_total < HEADPHONES_MAX_ADC_VALUE_KEY_PRESSED_C) {
        return HeadphonesStatusKeyPressedC; // ~470 Ohm (Function C)
    }
    return HeadphonesStatusNone;
}

bool headphones_update(HeadphonesStatus* status) {
    furi_check(headphones_instance);
    HeadphonesStatus old_state = headphones_instance->state;

    if(furi_hal_gpio_read(headphones_instance->headphone_detect_pin)) {
        if(headphones_instance->state & HeadphonesStatusDisconnected) {
            headphones_instance->state |= HeadphonesStatusConnected;
            headphones_instance->state &= ~HeadphonesStatusDisconnected;
            *status = headphones_instance->state;
            return true;
        }
    } else {
        headphones_instance->state = HeadphonesStatusDisconnected;
        headphones_instance->max_mic_bias_v = HEADPHONES_MBIAS_V;
        if(old_state != headphones_instance->state) {
            *status = headphones_instance->state;
            return true;
        }
        return false; // No need to check keys if headphones are disconnected
    }

    uint16_t adc_value = furi_hal_adc_read(headphones_instance->headphone_key_pin);
    uint8_t double_check = 4;
    
    do {
        furi_delay_ms(HEADPHONES_ADC_DEBOUNCE_MS);
        uint16_t adc_value_temp = furi_hal_adc_read(headphones_instance->headphone_key_pin);
        if(ADC_DIFF(adc_value, adc_value_temp) < HEADPHONES_ADC_VALUE_DELTA) {
            break;
        } else {
            adc_value = adc_value_temp;
        }
    } while(double_check--);

    if(double_check == 0) {
        HEADPHONES_DEBUG(TAG, "ADC value fluctuating too much, ignoring this reading. Last stable value: %u", adc_value);
        return false;
    }

    float adc_voltage = adc_value * furi_hal_adc_conversion_factor();

    HeadphonesStatus button_state = headphones_detect_button(adc_voltage);
    headphones_instance->state &= ~HEADPHONES_STATUS_KEY_PRESSED_MASK; // Clear all key pressed states
    if(button_state) {
        headphones_instance->state |= button_state;
    } else {
        headphones_instance->state |= HeadphonesStatusMicrophoneConnected;
        if(adc_voltage > headphones_instance->max_mic_bias_v) {
            headphones_instance->max_mic_bias_v = adc_voltage;
            HEADPHONES_DEBUG(TAG, "New max ADC voltage observed: %.4f V", headphones_instance->max_mic_bias_v);
        }
    }

    if(old_state != headphones_instance->state) {
        *status = headphones_instance->state;
        return true;
    }
    return false;
}
