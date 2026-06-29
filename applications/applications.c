#include "furi.h"
#include "applications.h"
const char* FLIPPER_AUTORUN_APP_NAME = "";

// services
extern int32_t haptic_srv(void* p);
extern int32_t test_peref_srv(void* p);
extern int32_t input_srv(void* p);
extern int32_t input_touch_srv(void* p);
extern int32_t i2c_intercom_srv(void* p);
extern int32_t i2c_negotiator_srv(void* p);
extern int32_t gui_srv(void* p);
extern int32_t desktop_srv(void* p);
extern int32_t led_srv(void* p);
extern int32_t usb_srv(void* p);
extern int32_t power_srv(void* p);
extern int32_t pd_srv(void* p);
extern int32_t headphones_srv(void* p);
extern int32_t usb_mux_srv(void* p);
extern int32_t cli_uart_srv(void* p);
extern int32_t cli_vcp_srv(void* p);

// applications
extern int32_t keypad_test_app(void* p);
extern int32_t touchpad_test_app(void* p);
extern int32_t haptic_test_app(void* p);
extern int32_t self_check_app(void* p);
extern int32_t font_test_app(void* p);
extern int32_t cli_on_system_start(void* p);

// CLI commands
extern void power_cli(PipeSide* pipe, FuriString* args, void* context);
extern void power_consumption_cli(PipeSide* pipe, FuriString* args, void* context);
extern void led_cli(PipeSide* pipe, FuriString* args, void* context);
extern void uart_echo_cli(PipeSide* pipe, FuriString* args, void* context);

const FlipperInternalApplication FLIPPER_SERVICES[] = {
    {
        .app = haptic_srv,
        .name = "HapticSrv",
        .appid = "haptic_srv",
        .stack_size = 1024,
        .flags = FlipperInternalApplicationFlagDefault,
    },
    {
        .app = input_srv,
        .name = "InputSrv",
        .appid = "input_srv",
        .stack_size = 1152,
        .flags = FlipperInternalApplicationFlagDefault,
    },
    {
        .app = power_srv,
        .name = "PowerSrv",
        .appid = "power_srv",
        .stack_size = 1024 * 2,
        .flags = FlipperInternalApplicationFlagDefault,
    },
    {
        .app = pd_srv,
        .name = "PdSrv",
        .appid = "pd_srv",
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
        .appid = "input_touch_srv",
        .stack_size = 1024,
        .flags = FlipperInternalApplicationFlagDefault,
    },
    {
        .app = i2c_intercom_srv,
        .name = "I2CIntercomSrv",
        .appid = "i2c_intercom_srv",
        .stack_size = 1024,
        .flags = FlipperInternalApplicationFlagDefault,
    },
    {
        .app = i2c_negotiator_srv,
        .name = "I2CNegotiatorSrv",
        .appid = "i2c_negotiator_srv",
        .stack_size = 1024 * 4,
        .flags = FlipperInternalApplicationFlagDefault,
    },
    // {
    //     .app = test_peref_srv,
    //     .name = "TestPerefSrv",
    //     .appid = "test_peref_srv",
    //     .stack_size = 1024,
    //     .flags = FlipperInternalApplicationFlagDefault,
    // },
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
        .app = led_srv,
        .name = "LedSrv",
        .appid = "led_srv",
        .stack_size = 1024,
        .flags = FlipperInternalApplicationFlagDefault,
    },
    {
        .app = usb_srv,
        .name = "UsbSrv",
        .appid = "usb_srv",
        .stack_size = 1024,
        .flags = FlipperInternalApplicationFlagDefault,
    },
    {
        .app = cli_uart_srv,
        .name = "CliUartSrv",
        .appid = "cli_uart_srv",
        .stack_size = 1024 * 2,
        .flags = FlipperInternalApplicationFlagDefault,
    },
    {
        .app = cli_vcp_srv,
        .name = "CliVcpSrv",
        .appid = "cli_vcp_srv",
        .stack_size = 1024 * 2,
        .flags = FlipperInternalApplicationFlagDefault,
    },
    {
        .app = headphones_srv,
        .name = "HeadphonesSrv",
        .appid = "headphones_srv",
        .stack_size = 1024,
        .flags = FlipperInternalApplicationFlagDefault,
    },
    {
        .app = usb_mux_srv,
        .name = "UsbMuxSrv",
        .appid = "usb_mux_srv",
        .stack_size = 1024,
        .flags = FlipperInternalApplicationFlagDefault,
    },
};
const size_t FLIPPER_SERVICES_COUNT = COUNT_OF(FLIPPER_SERVICES);

const FlipperInternalApplication FLIPPER_APPS[] = {
    {
        .app = self_check_app,
        .name = "Self Check",
        .appid = "self_check",
        .stack_size = 2048,
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
    {
        .app = haptic_test_app,
        .name = "Haptic Test",
        .appid = "haptic_test",
        .stack_size = 2048,
        .flags = FlipperInternalApplicationFlagDefault,
    },
    {
        .app = font_test_app,
        .name = "Font Test",
        .appid = "font_test",
        .stack_size = 2048,
        .flags = FlipperInternalApplicationFlagDefault,
    },
};
const size_t FLIPPER_APPS_COUNT = COUNT_OF(FLIPPER_APPS);

const FlipperInternalApplication FLIPPER_AUTORUN_APPS[] = {
    {
        .app = self_check_app,
        .name = "Self Check",
        .appid = "self_check",
        .stack_size = 2048,
        .flags = FlipperInternalApplicationFlagDefault,
    },
    {
        .app = cli_on_system_start,
        .name = "CliOnSystemStart",
        .appid = "cli_on_system_start",
        .stack_size = 1024 * 2,
        .flags = FlipperInternalApplicationFlagDefault,
    },
};
const size_t FLIPPER_AUTORUN_APPS_COUNT = COUNT_OF(FLIPPER_AUTORUN_APPS);

const FlipperInternalCommandApplication FLIPPER_CLI_COMMANDS[] = {
    {
        .callback = power_cli,
        .name = "power",
        .stack_size = 1024 * 4,
        .flags = CliCommandFlagParallelSafe,
    },
    {
        .callback = power_consumption_cli,
        .name = "power_consumption",
        .stack_size = 1024,
        .flags = CliCommandFlagParallelSafe,
    },
    {
        .callback = led_cli,
        .name = "led",
        .stack_size = 1024,
        .flags = CliCommandFlagParallelSafe,
    },
    {
        .callback = uart_echo_cli,
        .name = "uart_echo",
        .stack_size = 1024 * 2,
        .flags = CliCommandFlagParallelSafe,
    },
};
const size_t FLIPPER_CLI_COMMANDS_COUNT = COUNT_OF(FLIPPER_CLI_COMMANDS);
