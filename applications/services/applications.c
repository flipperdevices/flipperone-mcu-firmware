#include "furi.h"
#include "applications.h"
const char* FLIPPER_AUTORUN_APP_NAME = "";
extern int32_t test_srv(void* p);
extern int32_t test_peref_srv(void* p);
extern int32_t input_srv(void* p);
extern int32_t test_input_srv(void* p);
extern int32_t uart_echo_app(void* p);
extern int32_t input_touch_srv(void* p);
extern int32_t display_srv(void* p);

const FlipperInternalApplication FLIPPER_SERVICES[] = {

    // {.app = test_srv,
    //  .name = "TestSrv",
    //  .appid = "test", 
    //  .stack_size = 1024,
    //  .flags = FlipperInternalApplicationFlagDefault },
    {.app = input_srv,
     .name = "InputSrv",
     .appid = "input_srv",
     .stack_size = 1024,
     .flags = FlipperInternalApplicationFlagDefault },
    {.app = test_input_srv,
     .name = "TestInputSrv",
     .appid = "test_input",
     .stack_size = 1024,
     .flags = FlipperInternalApplicationFlagDefault },
    // {.app = uart_echo_app,
    //  .name = "UartEcho",
    //  .appid = "uart_echo",
    //  .stack_size = 2048,
    //  .flags = FlipperInternalApplicationFlagDefault },
    {.app = input_touch_srv,
     .name = "InputTouchSrv",
     .appid = "input_touch",
     .stack_size = 1024,
     .flags = FlipperInternalApplicationFlagDefault },
    {.app = display_srv,
     .name = "DisplaySrv",
     .appid = "display",
     .stack_size = 2048,
     .flags = FlipperInternalApplicationFlagDefault },
    {.app = test_peref_srv,
     .name = "TestPerefSrv",
     .appid = "test_peref",
     .stack_size = 1024,
     .flags = FlipperInternalApplicationFlagDefault },
};
const size_t FLIPPER_SERVICES_COUNT = COUNT_OF(FLIPPER_SERVICES);
