#include "led_cli.h"

#include <cli/args.h>
#include <led/led.h>

typedef struct {
    const char* name;
    FuriBspLedType type;
} LedTypeName;

static const LedTypeName cli_led_type_names[] = {
    {"Net", FuriBspLedTypeNet},
    {"WiFi", FuriBspLedTypeWiFi},
    {"Eth2", FuriBspLedTypeEth2},
    {"Eth1", FuriBspLedTypeEth1},
    {"Power", FuriBspLedTypePower},
    {"BatteryOutline", FuriBspLedTypeBatteryOutline},
    {"BatteryWatt1", FuriBspLedTypeBatteryWatt1},
    {"BatteryWatt2", FuriBspLedTypeBatteryWatt2},
    {"BatteryWatt3", FuriBspLedTypeBatteryWatt3},
    {"BatteryWatt4", FuriBspLedTypeBatteryWatt4},
    {"UsbCharging", FuriBspLedTypeUsbCharging},
    {"UsbWatt1", FuriBspLedTypeUsbWatt1},
    {"UsbWatt2", FuriBspLedTypeUsbWatt2},
    {"UsbWatt3", FuriBspLedTypeUsbWatt3},
    {"UsbWatt4", FuriBspLedTypeUsbWatt4},
    {"BatteryCenter", FuriBspLedTypeBatteryCenter},
    {"LineAllOff", FuriBspLedTypeAllOff},
    // test-only: the two halves of the battery outline, normally driven together
    {"BatteryOutline1", FuriBspLedTypeBatteryOutline1},
    {"BatteryOutline2", FuriBspLedTypeBatteryOutline2},
};

_Static_assert(
    COUNT_OF(cli_led_type_names) == (size_t)FuriBspLedTypeBatteryOutline2 + 1,
    "cli_led_type_names must have one entry per FuriBspLedType, see furi_bsp_led_type.h");

static void cli_command_led_help(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    UNUSED(args);
    UNUSED(context);
    printf("Usage: led <LED_TYPE> <COLOR>\r\n");
    printf("Where <LED_TYPE> is:\r\n");
    for(size_t i = 0; i < COUNT_OF(cli_led_type_names); i++) {
        const LedTypeName* led_type_name = &cli_led_type_names[i];
        printf("\t%-15s %d\r\n", led_type_name->name, (int)led_type_name->type);
    }
    printf(
        "Where <COLOR> is:\r\n"
        "\tred \t\t0\r\n"
        "\tgreen \t\t1\r\n"
        "\tblue \t\t2\r\n"
        "\tyellow \t\t3\r\n"
        "\torange \t\t4\r\n"
        "\tlight_blue \t5\r\n"
        "\tblack \t\t6\r\n"
        "\twhite \t\t7\r\n");
}

LedColor cli_led_colors[] = {
    LED_COLOR_RED,
    LED_COLOR_GREEN,
    LED_COLOR_BLUE,
    LED_COLOR_YELLOW,
    LED_COLOR_ORANGE,
    LED_COLOR_LIGHT_BLUE,
    LED_COLOR_BLACK,
    LED_COLOR_WHITE,
};

void led_cli(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    UNUSED(context);

    if(furi_string_size(args) < 1) {
        cli_command_led_help(pipe, args, context);
        return;
    }

    int led_type = 0;
    int color = 0;
    if(!args_read_int_and_trim(args, &led_type)) {
        cli_command_led_help(pipe, args, context);
        return;
    }
    // LED type is referenced by its FuriBspLedType value
    if(led_type < 0 || led_type > (int)FuriBspLedTypeBatteryOutline2) {
        cli_command_led_help(pipe, args, context);
        return;
    }
    const FuriBspLedType type = (FuriBspLedType)led_type;

    if(furi_string_size(args) < 1 && type != FuriBspLedTypeAllOff) {
        cli_command_led_help(pipe, args, context);
        return;
    }

    if(type != FuriBspLedTypeAllOff) {
        if(!args_read_int_and_trim(args, &color)) {
            cli_command_led_help(pipe, args, context);
            return;
        }
        if(color < 0 || color >= sizeof(cli_led_colors) / sizeof(LedColor)) {
            cli_command_led_help(pipe, args, context);
            return;
        }
    }

    Led* led = furi_record_open(RECORD_LEDS);
    led_set_color_single(led, type, cli_led_colors[color]);
    furi_record_close(RECORD_LEDS);
}
