#pragma once
#include <furi.h>
#include <hardware/gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Number of gpio on one port
 */
#define GPIO_NUMBER (NUM_BANK0_GPIOS)

/**
 * Interrupt callback prototype
 */
typedef void (*GpioExtiCallback)(void* ctx);

typedef enum {
    GpioConditionRise,
    GpioConditionFall,
    GpioConditionRiseFall,
} GpioCondition;

/**
 * Gpio interrupt type
 */
typedef struct {
    GpioExtiCallback callback;
    void* context;
    GpioCondition condition;
} GpioInterrupt;

/**
 * Gpio modes
 */
typedef enum {
    GpioModeInput,
    GpioModeOutputPushPull,
    GpioModeOutputOpenDrain,
    GpioModeAnalog,
} GpioMode;

/**
 * Gpio pull modes
 */
typedef enum {
    GpioPullNo,
    GpioPullUp,
    GpioPullDown,
} GpioPull;

/**
 * Gpio speed modes
 */
typedef enum {
    GpioSpeedLow, /**< Slew rate limiting enabled */
    GpioSpeedFast, /**< Slew rate limiting disabled */
} GpioSpeed;

/**
* Gpio Drive strength levels for GPIO outputs.
*/
typedef enum {
    GpioDriveStrengthLow, /**< 2 mA nominal drive strength */
    GpioDriveStrengthMedium, /**< 4 mA nominal drive strength */
    GpioDriveStrengthHigh, /**< 8 mA nominal drive strength */
    GpioDriveStrengthVeryHigh, /**< 12 mA nominal drive strength */
} GpioDriveStrength;

/**
 * Gpio alternate functions
 */
typedef enum {
    GpioAltFn0Hstx = GPIO_FUNC_HSTX, /**< Select HSTX as GPIO pin function */
    GpioAltFn1Spi = GPIO_FUNC_SPI, /**< Select SPI as GPIO pin function */
    GpioAltFn2Uart = GPIO_FUNC_UART, /**< Select UART as GPIO pin function */
    GpioAltFn3I2c = GPIO_FUNC_I2C, /**< Select I2C as GPIO pin function */
    GpioAltFn4Pwm = GPIO_FUNC_PWM, /**< Select PWM as GPIO pin function */
    GpioAltFn5Sio = GPIO_FUNC_SIO, /**< Select SIO as GPIO pin function */
    GpioAltFn6Pio0 = GPIO_FUNC_PIO0, /**< Select PIO0 as GPIO pin function */
    GpioAltFn7Pio1 = GPIO_FUNC_PIO1, /**< Select PIO1 as GPIO pin function */
    GpioAltFn8Pio2 = GPIO_FUNC_PIO2, /**< Select PIO2 as GPIO pin function */
    GpioAltFn9Gpck = GPIO_FUNC_GPCK, /**< Select GPCK as GPIO pin function */
    GpioAltFn10Usb = GPIO_FUNC_USB, /**< Select USB as GPIO pin function */
    GpioAltFn11UartAux = GPIO_FUNC_UART_AUX, /**< Select UART_AUX as GPIO pin function */
    GpioAltFnUnused = GPIO_FUNC_NULL, /**< Select NULL as GPIO pin function */
} GpioAltFn;

/**
 * Gpio structure
 */
typedef struct {
    uint pin; /**< Pin number */
} GpioPin;

/**
 * Initialize GPIO interrupts
 */
void furi_hal_gpio_interrupt_init(void);

/**
 * GPIO initialization function, simple version
 * @param gpio  GpioPin
 * @param mode  GpioMode
 */
void furi_hal_gpio_init_simple(const GpioPin* gpio, const GpioMode mode);

/**
 * GPIO initialization function, normal version
 * @param gpio  GpioPin
 * @param mode  GpioMode
 * @param pull  GpioPull
 * @param speed GpioSpeed
 */
void furi_hal_gpio_init(const GpioPin* gpio, const GpioMode mode, const GpioPull pull, const GpioSpeed speed);

/**
 * GPIO initialization function, extended version
 * @param gpio  GpioPin
 * @param mode  GpioMode
 * @param pull  GpioPull
 * @param speed GpioSpeed
 * @param alt_fn GpioAltFn
 */
void furi_hal_gpio_init_ex(const GpioPin* gpio, const GpioMode mode, const GpioPull pull, const GpioSpeed speed, const GpioAltFn alt_fn);

/**
 * Set GPIO drive strength
* @param gpio GpioPin
* @param strength GpioDriveStrength
*/
static FURI_ALWAYS_INLINE void furi_hal_gpio_set_drive_strength(const GpioPin* gpio, GpioDriveStrength strength) {
    furi_check(gpio->pin <= NUM_BANK0_GPIOS);

    const enum gpio_drive_strength drive_strength = (strength == GpioDriveStrengthLow)      ? GPIO_DRIVE_STRENGTH_2MA :
                                                    (strength == GpioDriveStrengthMedium)   ? GPIO_DRIVE_STRENGTH_4MA :
                                                    (strength == GpioDriveStrengthHigh)     ? GPIO_DRIVE_STRENGTH_8MA :
                                                    (strength == GpioDriveStrengthVeryHigh) ? GPIO_DRIVE_STRENGTH_12MA :
                                                                                              GPIO_DRIVE_STRENGTH_2MA;

    gpio_set_drive_strength(gpio->pin, drive_strength);
}

static FURI_ALWAYS_INLINE void furi_hal_gpio_set_function(const GpioPin* gpio, GpioAltFn alt_fn) {
    furi_check(gpio->pin <= NUM_BANK0_GPIOS);
    gpio_set_function(gpio->pin, (gpio_function_t)alt_fn);
}

/**
 * Add and enable interrupt
 * @param gpio GpioPin
 * @param condition GpioCondition
 * @param cb   GpioExtiCallback
 * @param ctx  context for callback
 */
void furi_hal_gpio_add_int_callback(const GpioPin* gpio, GpioCondition condition, GpioExtiCallback cb, void* ctx);

/**
 * Enable interrupt
 * @param gpio GpioPin
 */
void furi_hal_gpio_enable_int_callback(const GpioPin* gpio);

/**
 * Disable interrupt
 * @param gpio GpioPin
 */
void furi_hal_gpio_disable_int_callback(const GpioPin* gpio);

/**
 * Remove interrupt
 * @param gpio GpioPin
 */
void furi_hal_gpio_remove_int_callback(const GpioPin* gpio);

/**
 * GPIO write pin
 * @param gpio  GpioPin
 * @param state true / false
 */
static FURI_ALWAYS_INLINE void furi_hal_gpio_write(const GpioPin* gpio, const bool state) {
    gpio_put(gpio->pin, state);
}

/**
 * GPIO write pin, open drain mode
 * @param gpio  GpioPin
 * @param state true (high level) / false (low level)
 */
static FURI_ALWAYS_INLINE void furi_hal_gpio_write_open_drain(const GpioPin* gpio, const bool state) {
    gpio_set_dir(gpio->pin, state);
}

/**
 * GPIO read pin
 * @param gpio GpioPin
 * @return true / false
 */
static FURI_ALWAYS_INLINE bool furi_hal_gpio_read(const GpioPin* gpio) {
    return gpio_get(gpio->pin);
}

#ifdef __cplusplus
}
#endif
