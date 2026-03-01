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
extern int32_t led_srv(void* p);
extern int32_t usb_srv(void* p);
extern int32_t cli_srv(void* p);
extern int32_t fusb302_srv(void* p);
extern int32_t power_menu_srv(void* p);

// applications
extern int32_t keypad_test_app(void* p);
extern int32_t touchpad_test_app(void* p);
extern int32_t cpu_app(void* p);
extern int32_t haptic_test_app(void* p);

const FlipperInternalApplication FLIPPER_SERVICES[] = {
    {
        .app = haptic_srv,
        .name = "HapticSrv",
        .appid = "haptic_srv",
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
        .app = fusb302_srv,
        .name = "Fusb302Srv",
        .appid = "fusb302_srv",
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
        .stack_size = 768,
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
        .app = cli_srv,
        .name = "CliSrv",
        .appid = "cli_srv",
        .stack_size = 1024 * 2,
        .flags = FlipperInternalApplicationFlagDefault,
    },
    {
        .app = power_menu_srv,
        .name = "PowerMenuSrv",
        .appid = "power_menu_srv",
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
    {
        .app = haptic_test_app,
        .name = "Haptic Test",
        .appid = "haptic_test",
        .stack_size = 2048,
        .flags = FlipperInternalApplicationFlagDefault,
    },
};
const size_t FLIPPER_APPS_COUNT = COUNT_OF(FLIPPER_APPS);
