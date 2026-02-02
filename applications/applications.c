#include "furi.h"
#include "applications.h"
const char* FLIPPER_AUTORUN_APP_NAME = "";

// services
extern int32_t haptic_srv(void* p);
extern int32_t test_srv(void* p);
extern int32_t test_peref_srv(void* p);
extern int32_t input_srv(void* p);
extern int32_t test_input_srv(void* p);
extern int32_t uart_echo_app(void* p);
extern int32_t input_touch_srv(void* p);
extern int32_t display_srv(void* p);
extern int32_t gui_srv(void* p);
extern int32_t desktop_srv(void* p);
extern int32_t status_lights_srv(void* p);

// applications
extern int32_t keypad_test_app(void* p);
extern int32_t touchpad_test_app(void* p);

const FlipperInternalApplication FLIPPER_SERVICES[] = {
    {
        .app = haptic_srv,
        .name = "HapticSrv",
        .appid = "haptic",
        .stack_size = 768,
        .flags = FlipperInternalApplicationFlagDefault,
    },
    // {
    //     .app = test_srv,
    //     .name = "TestSrv",
    //     .appid = "test",
    //     .stack_size = 1024,
    //     .flags = FlipperInternalApplicationFlagDefault,
    // },
    {
        .app = input_srv,
        .name = "InputSrv",
        .appid = "input_srv",
        .stack_size = 1024,
        .flags = FlipperInternalApplicationFlagDefault,
    },
    // {
    //     .app = test_input_srv,
    //     .name = "TestInputSrv",
    //     .appid = "test_input",
    //     .stack_size = 512,
    //     .flags = FlipperInternalApplicationFlagDefault,
    // },
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
    // {
    //     .app = display_srv,
    //     .name = "DisplaySrv",
    //     .appid = "display",
    //     .stack_size = 1024,
    //     .flags = FlipperInternalApplicationFlagDefault,
    // },
    // {
    //     .app = test_peref_srv,
    //     .name = "TestPerefSrv",
    //     .appid = "test_peref",
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
        .app = status_lights_srv,
        .name = "StatusLightsSrv",
        .appid = "status_lights",
        .stack_size = 1024,
        .flags = FlipperInternalApplicationFlagDefault,
    },
};
const size_t FLIPPER_SERVICES_COUNT = COUNT_OF(FLIPPER_SERVICES);

const FlipperInternalApplication FLIPPER_APPS[] = {
    {
        .app = keypad_test_app,
        .name = "KeypadTest",
        .appid = "keypad_test",
        .stack_size = 2048,
        .flags = FlipperInternalApplicationFlagDefault,
    },
    {
        .app = touchpad_test_app,
        .name = "TouchpadTest",
        .appid = "touchpad_test",
        .stack_size = 2048,
        .flags = FlipperInternalApplicationFlagDefault,
    },
};
const size_t FLIPPER_APPS_COUNT = COUNT_OF(FLIPPER_APPS);
