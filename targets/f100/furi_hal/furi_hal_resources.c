#include <furi_hal_resources.h>

void furi_hal_resources_init(void){
    
}

void furi_hal_resources_init_early(void) {
   
}

void furi_hal_resources_deinit_early(void) {
    // Set all pins to input (as far as SIO is concerned)
    gpio_set_dir_all_bits(0);
    for (int i = 2; i < NUM_BANK0_GPIOS; ++i) {
        gpio_set_function(i, GpioAltFnUnused);
        if (i > NUM_BANK0_GPIOS - NUM_ADC_CHANNELS) {
            gpio_disable_pulls(i);
            gpio_set_input_enabled(i, false);
        }
    }
}
