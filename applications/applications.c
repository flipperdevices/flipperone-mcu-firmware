#include "furi.h"
#include "applications.h"
const char* FLIPPER_AUTORUN_APP_NAME = "";

// services
extern int32_t haptic_srv(void* p);
extern int32_t test_peref_srv(void* p);
extern int32_t input_srv(void* p);
extern int32_t uart_echo_app(void* p);
extern int32_t input_touch_srv(void* p);
extern int32_t gui_srv(void* p);
extern int32_t desktop_srv(void* p);
extern int32_t status_lights_srv(void* p);
extern int32_t usb_srv(void* p);
extern int32_t power_srv(void* p);
extern int32_t cli_srv(void* p);
extern int32_t fusb302_srv(void* p);
extern int32_t power_menu(void* p);

// applications
extern int32_t keypad_test_app(void* p);
extern int32_t touchpad_test_app(void* p);
extern int32_t cpu_app(void* p);
extern int32_t haptic_test_app(void* p);

// CLI commands
extern void power_cli(Cli* cli, FuriString* args, void* context);

const FlipperInternalApplication FLIPPER_SERVICES[] = {
    {
        .app = haptic_srv,
        .name = "HapticSrv",
        .appid = "haptic",
        .stack_size = 768,
        .flags = FlipperInternalApplicationFlagDefault,
    },
    {
        .app = input_srv,
        .name = "InputSrv",
        .appid = "input_srv",
        .stack_size = 1024,
        .flags = FlipperInternalApplicationFlagDefault,
    },
    {
        .app = power_srv,
        .name = "PowerSrv",
        .appid = "power",
        .stack_size = 1024,
        .flags = FlipperInternalApplicationFlagDefault,
    },
    {
        .app = fusb302_srv,
        .name = "Fusb302Srv",
        .appid = "fusb302",
        .stack_size = 1024,
        .flags = FlipperInternalApplicationFlagDefault,
    },
    // {
    //     .app = uart_echo_app,
    //     .name = "UartEcho",
    //     .appid = "uart_echo",
    //     .stack_size = 2048,
    //     .flags = FlipperInternalApplicationFlagDefault,
    // },
    {
        .app = input_touch_srv,
        .name = "InputTouchSrv",
        .appid = "input_touch",
        .stack_size = 768,
        .flags = FlipperInternalApplicationFlagDefault,
    },
    {
        .app = test_peref_srv,
        .name = "TestPerefSrv",
        .appid = "test_peref",
        .stack_size = 1024,
        .flags = FlipperInternalApplicationFlagDefault,
    },
    {
        .app = gui_srv,
        .name = "GuiSrv",
        .appid = "gui_srv",
        .stack_size = 1024 * 16,
        .flags = FlipperInternalApplicationFlagDefault,
    },
    {
        .app = desktop_srv,
        .name = "DesktopSrv",
        .appid = "desktop_srv",
        .stack_size = 1024 * 16,
        .flags = FlipperInternalApplicationFlagDefault,
    },
    {
        .app = status_lights_srv,
        .name = "StatusLightsSrv",
        .appid = "status_lights",
        .stack_size = 1024,
        .flags = FlipperInternalApplicationFlagDefault,
    },
    {
        .app = usb_srv,
        .name = "UsbSrv",
        .appid = "usb",
        .stack_size = 1024,
        .flags = FlipperInternalApplicationFlagDefault,
    },
    {
        .app = cli_srv,
        .name = "CliSrv",
        .appid = "cli",
        .stack_size = 1024 * 2,
        .flags = FlipperInternalApplicationFlagDefault,
    },
    {
        .app = power_menu,
        .name = "PowerMenu",
        .appid = "power_menu",
        .stack_size = 1024 * 4,
        .flags = FlipperInternalApplicationFlagDefault,
    },
};
const size_t FLIPPER_SERVICES_COUNT = COUNT_OF(FLIPPER_SERVICES);

const FlipperInternalApplication FLIPPER_APPS[] = {
    {
        .app = cpu_app,
        .name = "CPU",
        .appid = "cpu",
        .stack_size = 4096,
        .flags = FlipperInternalApplicationFlagDefault,
    },
    {
        .app = keypad_test_app,
        .name = "Keypad Test",
        .appid = "keypad_test",
        .stack_size = 2048,
        .flags = FlipperInternalApplicationFlagDefault,
    },
    {
        .app = touchpad_test_app,
        .name = "Touchpad Test",
        .appid = "touchpad_test",
        .stack_size = 2048,
        .flags = FlipperInternalApplicationFlagDefault,
    },
};
const size_t FLIPPER_APPS_COUNT = COUNT_OF(FLIPPER_APPS);

const FlipperInternalCommandApplication FLIPPER_CLI_COMMANDS[] = {
    {
        .callback = power_cli,
        .name = "power",
        .stack_size = 1048,
        .flags = CliCommandFlagParallelSafe,
    },
};
const size_t FLIPPER_CLI_COMMANDS_COUNT = COUNT_OF(FLIPPER_CLI_COMMANDS);
