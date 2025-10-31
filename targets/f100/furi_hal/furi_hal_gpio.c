#include <furi_hal_gpio.h>
#include <hardware/exception.h>
#include <hardware/adc.h>

static volatile GpioInterrupt gpio_interrupt[GPIO_NUMBER];

void furi_hal_gpio_callback(uint gpio, uint32_t event_mask);

void furi_hal_gpio_init_simple(const GpioPin* gpio, const GpioMode mode) {
    furi_hal_gpio_init(gpio, mode, GpioPullNo, GpioSpeedLow);
}

void furi_hal_gpio_init(const GpioPin* gpio, const GpioMode mode, const GpioPull pull, const GpioSpeed speed) {
    // // we cannot set alternate mode in this function
    // furi_check(mode != GpioModeAltFunctionPushPull);
    // furi_check(mode != GpioModeAltFunctionOpenDrain);

    furi_hal_gpio_init_ex(gpio, mode, pull, speed, GpioAltFn5Sio);
}

void furi_hal_gpio_init_ex(const GpioPin* gpio, const GpioMode mode, const GpioPull pull, const GpioSpeed speed, const GpioAltFn alt_fn) {
    // Configure gpio with interrupts disabled
    FURI_CRITICAL_ENTER();

    gpio_set_function(gpio->pin, alt_fn);

    // Set gpio speed
    switch(speed) {
    case GpioSpeedLow:
        gpio_set_slew_rate(gpio->pin, GPIO_SLEW_RATE_SLOW);
        break;
    case GpioSpeedFast:
        gpio_set_slew_rate(gpio->pin, GPIO_SLEW_RATE_FAST);
        break;
    default:
        furi_crash("Incorrect GpioSpeed");
        break;
    }

    // Set gpio pull mode
    switch(pull) {
    case GpioPullNo:
        gpio_set_pulls(gpio->pin, false, false);
        break;
    case GpioPullUp:
        gpio_set_pulls(gpio->pin, true, false);
        break;
    case GpioPullDown:
        gpio_set_pulls(gpio->pin, false, true);
        break;
    default:
        furi_crash("Incorrect GpioPull");
        break;
    }

    // Set gpio mode
    switch(mode) {
    case GpioModeInput:
        gpio_set_dir(gpio->pin, GPIO_IN);
        gpio_set_input_hysteresis_enabled(gpio->pin, true);
        break;
    case GpioModeOutputPushPull:
        gpio_set_dir(gpio->pin, GPIO_OUT);
        gpio_set_input_hysteresis_enabled(gpio->pin, false);
        break;
    case GpioModeOutputOpenDrain:
        // Open drain not supported directly, set as output for now
        //Todo: implement open drain using GPIO registers directly
        gpio_set_dir(gpio->pin, GPIO_OUT);
        gpio_set_input_hysteresis_enabled(gpio->pin, false);
        break;
    case GpioModeAnalog:
        adc_gpio_init(gpio->pin);
        gpio_set_input_hysteresis_enabled(gpio->pin, false);
        break;
    default:
        furi_crash("Incorrect GpioMode");
        break;
    }

    FURI_CRITICAL_EXIT();
}

void furi_hal_gpio_add_int_callback(const GpioPin* gpio, GpioCondition condition, GpioExtiCallback cb, void* ctx) {
    furi_check(gpio->pin <= NUM_BANK0_GPIOS);
    furi_check(cb);

    FURI_CRITICAL_ENTER();
    furi_check(gpio_interrupt[gpio->pin].callback == NULL);
    gpio_interrupt[gpio->pin].callback = cb;
    gpio_interrupt[gpio->pin].context = ctx;

    gpio_set_irq_enabled_with_callback(
        gpio->pin,
        (condition == GpioConditionRise)     ? GPIO_IRQ_EDGE_RISE :
        (condition == GpioConditionFall)     ? GPIO_IRQ_EDGE_FALL :
        (condition == GpioConditionRiseFall) ? GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL :
                                               0,
        true,
        furi_hal_gpio_callback);

    FURI_CRITICAL_EXIT();
}

void furi_hal_gpio_enable_int_callback(const GpioPin* gpio) {
    furi_check(gpio->pin <= NUM_BANK0_GPIOS);

    FURI_CRITICAL_ENTER();

    irq_set_enabled(gpio->pin, true);

    FURI_CRITICAL_EXIT();
}

void furi_hal_gpio_disable_int_callback(const GpioPin* gpio) {
    furi_check(gpio->pin <= NUM_BANK0_GPIOS);

    FURI_CRITICAL_ENTER();

    irq_set_enabled(gpio->pin, false);

    FURI_CRITICAL_EXIT();
}

void furi_hal_gpio_remove_int_callback(const GpioPin* gpio) {
    furi_check(gpio->pin <= NUM_BANK0_GPIOS);

    FURI_CRITICAL_ENTER();
    gpio_set_irq_enabled_with_callback(gpio->pin, 0, false, NULL);
    gpio_interrupt[gpio->pin].callback = NULL;
    gpio_interrupt[gpio->pin].context = NULL;

    FURI_CRITICAL_EXIT();
}

FURI_ALWAYS_INLINE static void furi_hal_gpio_int_call(uint16_t pin_num) {
    if(gpio_interrupt[pin_num].callback) {
        gpio_interrupt[pin_num].callback(gpio_interrupt[pin_num].context);
    }
}

void __isr __not_in_flash_func(furi_hal_gpio_callback)(uint gpio, uint32_t event_mask) {
    furi_hal_gpio_int_call(gpio);
}
