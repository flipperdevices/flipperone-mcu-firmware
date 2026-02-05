#include "furi_hal_gpio.h"
#include <furi.h>
#include <furi_hal_resources.h>
#include <furi_hal_power.h>

#define TAG "TestSrv"

typedef struct {
    bool stop;
    uint32_t counter;
} InputTest;


int32_t test_srv(void* p) {
    UNUSED(p);

    InputTest* instance = malloc(sizeof(InputTest));
    //furi_hal_gpio_init_simple(&gpio_key_down, GpioModeOutputOpenDrain);
    bool gpio_key_down_state = false;
    while(!instance->stop) {
        FURI_LOG_I(TAG, "Test message");
        furi_delay_ms(5000);
        // gpio_key_down_state = !gpio_key_down_state;
        // furi_hal_gpio_write_open_drain(&gpio_key_down, gpio_key_down_state);
        // furi_hal_power_insomnia_enter();
        // FURI_LOG_I(TAG, " GPIO Key Down state: %d", gpio_key_down_state);
        // furi_hal_power_insomnia_exit();
    }

    free(instance);
    return 0;
}