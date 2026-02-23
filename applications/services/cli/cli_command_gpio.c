#include "cli_command_gpio.h"

#include <furi_hal_resources.h>
#include <args.h>

typedef struct {
    const char* name;
    const GpioPin* pin;
} CliGpioPinNamePair;

static bool cli_command_gpio_parse_value(FuriString* args, uint8_t* value) {
    bool ret = false;

    int pin_mode; //-V779
    if(args_read_int_and_trim(args, &pin_mode) && (pin_mode == 0 || pin_mode == 1)) {
        ret = true;
        *value = pin_mode;
    }

    return ret;
}

static const CliGpioPinNamePair gpios[] = {
    {
        .name = "mcu_gpio0",
        .pin = &gpio_mcu_gpio0,
    },
    {
        .name = "mcu_gpio1",
        .pin = &gpio_mcu_gpio1,
    },
};

static void gpio_print_pins(void) {
    uint8_t n = COUNT_OF(gpios);
    for(uint8_t i = 0; i < n; i++) {
        const char* format = i + 1 == n ? "%s" : "%s, ";
        printf(format, gpios[i].name);
    }
}

static void cli_command_gpio_print_usage(void) {
    printf("Usage:\r\n");
    printf("gpio <pin_name> <0|1>\t - Set gpio value\r\n");
    printf("Pins: ");
    gpio_print_pins();
}

static bool cli_command_gpio_parse_pin(FuriString* args, const CliGpioPinNamePair** pin) {
    bool result = false;
    FuriString* pin_name = furi_string_alloc();
    do {
        if(!args_read_string_and_trim(args, pin_name)) break;

        for(uint8_t i = 0; i < COUNT_OF(gpios); i++) {
            if(!furi_string_equal_str(pin_name, gpios[i].name)) continue;
            *pin = &gpios[i];
            result = true;
            break;
        }
    } while(false);
    furi_string_free(pin_name);

    return result;
}

void cli_command_gpio(Cli* cli, FuriString* args, void* context) {
    UNUSED(cli);
    UNUSED(context);

    do {
        const CliGpioPinNamePair* pin_description = NULL;
        if(!cli_command_gpio_parse_pin(args, &pin_description)) {
            cli_command_gpio_print_usage();
            break;
        }

        uint8_t value;
        if(!cli_command_gpio_parse_value(args, &value)) {
            cli_command_gpio_print_usage();
            break;
        }

        furi_hal_gpio_write(pin_description->pin, value);
        printf("Pin %s => %u", pin_description->name, value);
    } while(false);
}
