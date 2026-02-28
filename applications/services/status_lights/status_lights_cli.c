#include "status_lights_cli.h"

#include <args.h>
#include <status_lights/status_lights.h>

static void status_lights_help(Cli* cli, FuriString* args, void* context) {
    UNUSED(cli);
    UNUSED(args);
    UNUSED(context);
    printf(
        "Usage: set_led <LED_TYPE> <COLOR>\r\n"
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

StatusLightsType cli_status_lights_types[] = {
    StatusLightsTypeNet,
    StatusLightsTypeWiFi,
    StatusLightsTypeEth2,
    StatusLightsTypeEth1,
    StatusLightsTypePower,
    StatusLightsTypeBatteryOutline,
    StatusLightsTypeBatteryWatt1,
    StatusLightsTypeBatteryWatt2,
    StatusLightsTypeBatteryWatt3,
    StatusLightsTypeBatteryWatt4,
    StatusLightsTypeUsbCharging,
    StatusLightsTypeUsbWatt1,
    StatusLightsTypeUsbWatt2,
    StatusLightsTypeUsbWatt3,
    StatusLightsTypeUsbWatt4,
    StatusLightsTypeBatteryCenter,
    StatusLightsTypeLineAllOff,
};

StatusLightsColor cli_status_lights_colors[] = {
    STATUS_LIGHTS_COLOR_RED,
    STATUS_LIGHTS_COLOR_GREEN,
    STATUS_LIGHTS_COLOR_BLUE,
    STATUS_LIGHTS_COLOR_YELLOW,
    STATUS_LIGHTS_COLOR_ORANGE,
    STATUS_LIGHTS_COLOR_LIGHT_BLUE,
    STATUS_LIGHTS_COLOR_BLACK,
};

void status_lights_cli(Cli* cli, FuriString* args, void* context) {
    UNUSED(cli);
    UNUSED(context);

    if(furi_string_size(args) < 1) {
        status_lights_help(cli, args, context);
        return;
    }

    int led_type = 0;
    int color = 0;
    if(!args_read_int_and_trim(args, &led_type)) {
        status_lights_help(cli, args, context);
        return;
    }
    if(led_type < 0 || led_type >= sizeof(cli_status_lights_types) / sizeof(StatusLightsType)) {
        status_lights_help(cli, args, context);
        return;
    }

    if(furi_string_size(args) < 1 && cli_status_lights_types[led_type] != StatusLightsTypeLineAllOff) {
        status_lights_help(cli, args, context);
        return;
    }

    if(cli_status_lights_types[led_type] != StatusLightsTypeLineAllOff) {
        if(!args_read_int_and_trim(args, &color)) {
            status_lights_help(cli, args, context);
            return;
        }
        if(color < 0 || color >= sizeof(cli_status_lights_colors) / sizeof(StatusLightsColor)) {
            status_lights_help(cli, args, context);
            return;
        }
    }

    StatusLights* status_lights = furi_record_open(RECORD_STATUS_LIGHTS);
    status_lights_notification(status_lights, cli_status_lights_types[led_type], cli_status_lights_colors[color]);
    furi_record_close(RECORD_STATUS_LIGHTS);
}
