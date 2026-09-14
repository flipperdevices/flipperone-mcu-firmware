#include "led_cli.h"

#include <cli/args.h>
#include <led/led.h>

static void cli_command_led_help(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    UNUSED(args);
    UNUSED(context);
    printf(
        "Usage: led <LED_TYPE> <COLOR>\r\n"
        "Where <LED_TYPE> is:\r\n"
        "\tNet \t\t0\r\n"
        "\tWiFi \t\t1\r\n"
        "\tEth2 \t\t2\r\n"
        "\tEth1 \t\t3\r\n"
        "\tPower \t\t4\r\n"
        "\tBatteryOutline\t5\r\n"
        "\tBatteryWatt1\t6\r\n"
        "\tBatteryWatt2\t7\r\n"
        "\tBatteryWatt3\t8\r\n"
        "\tBatteryWatt4\t9\r\n"
        "\tUsbCharging \t10\r\n"
        "\tUsbWatt1 \t11\r\n"
        "\tUsbWatt2 \t12\r\n"
        "\tUsbWatt3 \t13\r\n"
        "\tUsbWatt4 \t14\r\n"
        "\tBatteryCenter\t15\r\n"
        "\tLineAllOff \t16\r\n");
    printf(
        "Where <COLOR> is:\r\n"
        "\tred \t\t0\r\n"
        "\tgreen \t\t1\r\n"
        "\tblue \t\t2\r\n"
        "\tyellow \t\t3\r\n"
        "\torange \t\t4\r\n"
        "\tlight_blue \t5\r\n"
        "\tblack \t\t6\r\n");
}

FuriBspLedType cli_led_types[] = {
    FuriBspLedTypeNet,
    FuriBspLedTypeWiFi,
    FuriBspLedTypeEth2,
    FuriBspLedTypeEth1,
    FuriBspLedTypePower,
    FuriBspLedTypeBatteryOutline,
    FuriBspLedTypeBatteryWatt1,
    FuriBspLedTypeBatteryWatt2,
    FuriBspLedTypeBatteryWatt3,
    FuriBspLedTypeBatteryWatt4,
    FuriBspLedTypeUsbCharging,
    FuriBspLedTypeUsbWatt1,
    FuriBspLedTypeUsbWatt2,
    FuriBspLedTypeUsbWatt3,
    FuriBspLedTypeUsbWatt4,
    FuriBspLedTypeBatteryCenter,
    FuriBspLedTypeAllOff,
};

LedColor cli_led_colors[] = {
    LED_COLOR_RED,
    LED_COLOR_GREEN,
    LED_COLOR_BLUE,
    LED_COLOR_YELLOW,
    LED_COLOR_ORANGE,
    LED_COLOR_LIGHT_BLUE,
    LED_COLOR_BLACK,
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
    if(led_type < 0 || led_type >= sizeof(cli_led_types) / sizeof(FuriBspLedType)) {
        cli_command_led_help(pipe, args, context);
        return;
    }

    if(furi_string_size(args) < 1 && cli_led_types[led_type] != FuriBspLedTypeAllOff) {
        cli_command_led_help(pipe, args, context);
        return;
    }

    if(cli_led_types[led_type] != FuriBspLedTypeAllOff) {
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
    led_set_color_single(led, cli_led_types[led_type], cli_led_colors[color]);
    furi_record_close(RECORD_LEDS);
}
