#include <status_lights/status_lights.h>
#include <status_lights/status_lights_notification.h>

#define NOTIFICATION_DECLARE(...) \
    &(StatusLightsNotification) { \
        __VA_ARGS__               \
    }

struct StatusLightsNotification {
    const StatusLightsType status_lights_type;
    const StatusLightsColor color;
};

const StatusLightsNotification* notification_all_leds_off[] = {
    NOTIFICATION_DECLARE(.status_lights_type = StatusLightsTypeLineAllOff),
    NULL,
};

const StatusLightsNotification* notification_power_red[] = {
    NOTIFICATION_DECLARE(.status_lights_type = StatusLightsTypePower, .color = STATUS_LIGHTS_COLOR_RED),
    NULL,
};

const StatusLightsNotification* notification_all_leds_on[] = {
    NOTIFICATION_DECLARE(.status_lights_type = StatusLightsTypeNet, .color = (StatusLightsColor){0, 0, 255}),
    NOTIFICATION_DECLARE(.status_lights_type = StatusLightsTypeWiFi, .color = (StatusLightsColor){0, 0, 255}),
    NOTIFICATION_DECLARE(.status_lights_type = StatusLightsTypeEth2, .color = (StatusLightsColor){0, 0, 255}),
    NOTIFICATION_DECLARE(.status_lights_type = StatusLightsTypeEth1, .color = (StatusLightsColor){0, 0, 255}),
    NOTIFICATION_DECLARE(.status_lights_type = StatusLightsTypePower, .color = (StatusLightsColor){0, 255, 0}),
    NOTIFICATION_DECLARE(.status_lights_type = StatusLightsTypeBatteryOutline, .color = (StatusLightsColor){0, 255, 0}),
    NOTIFICATION_DECLARE(.status_lights_type = StatusLightsTypeBatteryWatt1, .color = (StatusLightsColor){255, 0, 0}),
    NOTIFICATION_DECLARE(.status_lights_type = StatusLightsTypeBatteryWatt2, .color = (StatusLightsColor){255, 0, 0}),
    NOTIFICATION_DECLARE(.status_lights_type = StatusLightsTypeBatteryWatt3, .color = (StatusLightsColor){255, 255, 0}),
    NOTIFICATION_DECLARE(.status_lights_type = StatusLightsTypeBatteryWatt4, .color = (StatusLightsColor){0, 255, 0}),
    NOTIFICATION_DECLARE(.status_lights_type = StatusLightsTypeUsbCharging, .color = (StatusLightsColor){255, 0, 0}),
    NOTIFICATION_DECLARE(.status_lights_type = StatusLightsTypeUsbWatt1, .color = (StatusLightsColor){255, 0, 0}),
    NOTIFICATION_DECLARE(.status_lights_type = StatusLightsTypeUsbWatt2, .color = (StatusLightsColor){255, 0, 0}),
    NOTIFICATION_DECLARE(.status_lights_type = StatusLightsTypeUsbWatt3, .color = (StatusLightsColor){255, 255, 0}),
    NOTIFICATION_DECLARE(.status_lights_type = StatusLightsTypeUsbWatt4, .color = (StatusLightsColor){0, 255, 0}),
    NOTIFICATION_DECLARE(.status_lights_type = StatusLightsTypeBatteryCenter, .color = (StatusLightsColor){0, 255, 0}),
    NULL,
};

const StatusLightsNotification* notification_all_leds_white[] = {
    NOTIFICATION_DECLARE(.status_lights_type = StatusLightsTypeNet, .color = (StatusLightsColor){255, 255, 255}),
    NOTIFICATION_DECLARE(.status_lights_type = StatusLightsTypeWiFi, .color = (StatusLightsColor){255, 255, 255}),
    NOTIFICATION_DECLARE(.status_lights_type = StatusLightsTypeEth2, .color = (StatusLightsColor){255, 255, 255}),
    NOTIFICATION_DECLARE(.status_lights_type = StatusLightsTypeEth1, .color = (StatusLightsColor){255, 255, 255}),
    NOTIFICATION_DECLARE(.status_lights_type = StatusLightsTypePower, .color = (StatusLightsColor){255, 255, 255}),
    NOTIFICATION_DECLARE(.status_lights_type = StatusLightsTypeBatteryOutline, .color = (StatusLightsColor){255, 255, 255}),
    NOTIFICATION_DECLARE(.status_lights_type = StatusLightsTypeBatteryWatt1, .color = (StatusLightsColor){255, 255, 255}),
    NOTIFICATION_DECLARE(.status_lights_type = StatusLightsTypeBatteryWatt2, .color = (StatusLightsColor){255, 255, 255}),
    NOTIFICATION_DECLARE(.status_lights_type = StatusLightsTypeBatteryWatt3, .color = (StatusLightsColor){255, 255, 255}),
    NOTIFICATION_DECLARE(.status_lights_type = StatusLightsTypeBatteryWatt4, .color = (StatusLightsColor){255, 255, 255}),
    NOTIFICATION_DECLARE(.status_lights_type = StatusLightsTypeUsbCharging, .color = (StatusLightsColor){255, 255, 255}),
    NOTIFICATION_DECLARE(.status_lights_type = StatusLightsTypeUsbWatt1, .color = (StatusLightsColor){255, 255, 255}),
    NOTIFICATION_DECLARE(.status_lights_type = StatusLightsTypeUsbWatt2, .color = (StatusLightsColor){255, 255, 255}),
    NOTIFICATION_DECLARE(.status_lights_type = StatusLightsTypeUsbWatt3, .color = (StatusLightsColor){255, 255, 255}),
    NOTIFICATION_DECLARE(.status_lights_type = StatusLightsTypeUsbWatt4, .color = (StatusLightsColor){255, 255, 255}),
    NOTIFICATION_DECLARE(.status_lights_type = StatusLightsTypeBatteryCenter, .color = (StatusLightsColor){255, 255, 255}),
    NULL,
};

void status_lights_notification_send(const StatusLightsNotification** notifications) {
    StatusLights* status_lights = furi_record_open(RECORD_STATUS_LIGHTS);
    const StatusLightsNotification* notification = notifications[0];
    while(notification) {
        status_lights_notification(status_lights, notification->status_lights_type, notification->color);
        notifications++;
        notification = notifications[0];
    }
    furi_record_close(RECORD_STATUS_LIGHTS);
}
