#pragma once
#include <furi.h>

#define RECORD_STATUS_LIGHTS "status_lights"
typedef struct StatusLights StatusLights;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} StatusLightsColor;

#define STATUS_LIGHTS_COLOR_RED        (StatusLightsColor){255, 0, 0}
#define STATUS_LIGHTS_COLOR_GREEN      (StatusLightsColor){0, 255, 0}
#define STATUS_LIGHTS_COLOR_BLUE       (StatusLightsColor){0, 0, 255}
#define STATUS_LIGHTS_COLOR_YELLOW     (StatusLightsColor){255, 255, 0}
#define STATUS_LIGHTS_COLOR_ORANGE     (StatusLightsColor){255, 165, 0}
#define STATUS_LIGHTS_COLOR_LIGHT_BLUE (StatusLightsColor){0x12, 0xCD, 0xD4}
#define STATUS_LIGHTS_COLOR_BLACK      (StatusLightsColor){0, 0, 0}

typedef enum {
    //line 1
    StatusLightsTypeNet,
    StatusLightsTypeWiFi,
    StatusLightsTypeEth2,
    StatusLightsTypeEth1,

    //line 2
    StatusLightsTypePower,
    StatusLightsTypeBatteryOutline, //2 status_lightss
    StatusLightsTypeBatteryWatt1 = StatusLightsTypeBatteryOutline + 2,
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

    StatusLightsTypeLine1Off,
    StatusLightsTypeLine2Off,
    StatusLightsTypeLine3Off,
    StatusLightsTypeLineAllOff,
} StatusLightsType;

typedef struct {
    const StatusLightsType status_lights_type;
    const StatusLightsColor color;
} StatusLightsNotificationSuqeueItem;

typedef struct {
    const StatusLightsNotificationSuqeueItem** notifications;
    const size_t notification_count;
} StatusLightsNotification;

#ifdef __cplusplus
extern "C" {
#endif

void status_lights_notification_led(StatusLights* instance, StatusLightsType status_lights_type, StatusLightsColor color);
void status_lights_notification(StatusLights* instance, const StatusLightsNotification** notifications);

#ifdef __cplusplus
}
#endif
