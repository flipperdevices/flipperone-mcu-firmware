#include <status_lights/status_lights_notification.h>

#define NOTIFICATION_DECLARE(...)           \
    &(StatusLightsNotificationSuqeueItem) { \
        __VA_ARGS__                         \
    }

const StatusLightsNotificationSuqeueItem* notification_all_leds_off_suqeue_item[] = {
    NOTIFICATION_DECLARE(.status_lights_type = StatusLightsTypeLineAllOff),
};

const size_t notification_all_leds_off_count = sizeof(notification_all_leds_off_suqeue_item) / sizeof(StatusLightsNotificationSuqeueItem*);

const StatusLightsNotification* notification_all_leds_off[] = {
    &(StatusLightsNotification){
        .notifications = notification_all_leds_off_suqeue_item,
        .notification_count = notification_all_leds_off_count,
    },
};

const StatusLightsNotificationSuqeueItem* notification_power_red_suqeue_item[] = {
    NOTIFICATION_DECLARE(.status_lights_type = StatusLightsTypePower, .color = STATUS_LIGHTS_COLOR_RED),
};

const size_t notification_power_red_count = sizeof(notification_power_red_suqeue_item) / sizeof(StatusLightsNotificationSuqeueItem*);

const StatusLightsNotification* notification_power_red[] = {
    &(StatusLightsNotification){
        .notifications = notification_power_red_suqeue_item,
        .notification_count = notification_power_red_count,
    },
};

const StatusLightsNotificationSuqeueItem* notification_all_leds_on_suqeue_item[] = {
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
};

const size_t notification_all_leds_on_count = sizeof(notification_all_leds_on_suqeue_item) / sizeof(StatusLightsNotificationSuqeueItem*);

const StatusLightsNotification* notification_all_leds_on[] = {
    &(StatusLightsNotification){
        .notifications = notification_all_leds_on_suqeue_item,
        .notification_count = notification_all_leds_on_count,
    },
};

const StatusLightsNotificationSuqeueItem* notification_all_leds_white_suqeue_item[] = {
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
};

const size_t notification_all_leds_white_count = sizeof(notification_all_leds_white_suqeue_item) / sizeof(StatusLightsNotificationSuqeueItem*);

const StatusLightsNotification* notification_all_leds_white[] = {
    &(StatusLightsNotification){
        .notifications = notification_all_leds_white_suqeue_item,
        .notification_count = notification_all_leds_white_count,
    },
};

void status_lights_notification_send(const StatusLightsNotification** notifications) {
    StatusLights* status_lights = furi_record_open(RECORD_STATUS_LIGHTS);
    status_lights_notification(status_lights, notifications);
    furi_record_close(RECORD_STATUS_LIGHTS);
}
