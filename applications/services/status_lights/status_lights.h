#pragma once
#include <furi.h>

#define RECORD_STATUS_LIGHTS "status_lights"
typedef struct StatusLights StatusLights;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} StatusLightsColor;

#define STATUS_LIGHTS_COLOR_RED        {255, 0, 0};
#define STATUS_LIGHTS_COLOR_GREEN      {0, 255, 0};
#define STATUS_LIGHTS_COLOR_BLUE       {0, 0, 255};
#define STATUS_LIGHTS_COLOR_YELLOW     {255, 255, 0};
#define STATUS_LIGHTS_COLOR_ORANGE     {255, 165, 0};
#define STATUS_LIGHTS_COLOR_LIGHT_BLUE {0x12, 0xCD, 0xD4};
#define STATUS_LIGHTS_COLOR_BLACK      {0, 0, 0};

typedef enum {
    //line 1
    StatusLightsTypeNet,
    StatusLightsTypeWiFi,
    StatusLightsTypeEth2,
    StatusLightsTypeEth1,

    //line 2
    StatusLightsTypePower,
    StatusLightsTypeBatteryOutline, //2 status_lightss
    StatusLightsTypeBatteryWatt1,
    StatusLightsTypeBatteryWatt2,
    StatusLightsTypeBatteryWatt3,
    StatusLightsTypeBatteryWatt4,

    //line 3
    StatusLightsTypeUsbCharging,
    StatusLightsTypeUsbWatt1,
    StatusLightsTypeUsbWatt2,
    StatusLightsTypeUsbWatt3,
    StatusLightsTypeUsbWatt4,
    StatusLightsTypeBatteryCenter,
} StatusLightsType;

#ifdef __cplusplus
extern "C" {
#endif

void status_lights_notification(StatusLights* instance, StatusLightsType status_lights_type, StatusLightsColor color);

#ifdef __cplusplus
}
#endif
